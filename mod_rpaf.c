/*
   Copyright 2011 Ask Bjørn Hansen

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "ap_release.h"
#if AP_SERVER_MAJORVERSION_NUMBER >= 2 && AP_SERVER_MINORVERSION_NUMBER >= 4
  #define DEF_IP   useragent_ip
  #define DEF_ADDR useragent_addr
  #define DEF_POOL pool
#else
  #define DEF_IP   connection->remote_ip
  #define DEF_ADDR connection->remote_addr
  #define DEF_POOL connection->pool
#endif

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_vhost.h"
#include "apr_strings.h"

#include <ctype.h> // isspace
#include <arpa/inet.h>

module AP_MODULE_DECLARE_DATA rpaf_module;
APR_DECLARE_OPTIONAL_FN(int, ssl_is_https, (conn_rec *));

typedef struct {
    int                enable;
    int                sethostname;
    int                sethttps;
    int                setport;
    const char         *headername;
    apr_array_header_t *proxy_ips;
    const char         *orig_scheme;
    const char         *https_scheme;
    int                orig_port;
    int                forbid_if_not_proxy;
} rpaf_server_cfg;

typedef struct {
    const char  *old_ip;
    request_rec *r;
} rpaf_cleanup_rec;

static void *rpaf_create_server_cfg(apr_pool_t *p, server_rec *s) {
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)apr_pcalloc(p, sizeof(rpaf_server_cfg));
    if (!cfg)
        return NULL;

    cfg->proxy_ips = apr_array_make(p, 10, sizeof(apr_ipsubnet_t *));
    cfg->enable = 0;
    cfg->sethostname = 0;
    cfg->forbid_if_not_proxy = 0;

    /* server_rec->server_scheme only available after 2.2.3 */
    #if AP_SERVER_MINORVERSION_NUMBER > 1 && AP_SERVER_PATCHLEVEL_NUMBER > 2
    cfg->orig_scheme = s->server_scheme;
    #endif

    cfg->https_scheme = apr_pstrdup(p, "https");
    cfg->orig_port = s->port;

    return (void *)cfg;
}

/* quick check for ipv4/6 likelihood; similar to Apache2.4 mod_remoteip check */
static int rpaf_looks_like_ip(const char *ip) {
    static const char ipv4_set[] = "0123456789./";
    static const char ipv6_set[] = "0123456789abcdef:/";

    /* zero length value is not valid */
    if (!*ip)
      return 0;

    const char *ptr    = ip;

    /* determine if this could be a IPv6 or IPv4 address */
    if (strchr(ip, ':'))
    {
        while(*ptr && strchr(ipv6_set, *ptr) != NULL)
            ++ptr;
    }
    else
    {
        while(*ptr && strchr(ipv4_set, *ptr) != NULL)
            ++ptr;
    }

    return (*ptr == '\0');
}

static const char *rpaf_set_proxy_ip(cmd_parms *cmd, void *dummy, const char *proxy_ip) {
    char *ip, *mask;
    apr_ipsubnet_t **sub;
    apr_status_t rv;
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config,
                                                                   &rpaf_module);

    if (rpaf_looks_like_ip(proxy_ip)) {
        ip = apr_pstrdup(cmd->temp_pool, proxy_ip);
        if (mask = ap_strchr(ip, '/')) {
            *mask++ = '\0';
        }
        sub = (apr_ipsubnet_t **)apr_array_push(cfg->proxy_ips);
        rv = apr_ipsubnet_create(sub, ip, mask, cmd->pool);

        if (rv != APR_SUCCESS) {
            char msgbuf[128];
            apr_strerror(rv, msgbuf, sizeof(msgbuf));
            return apr_pstrcat(cmd->pool, "mod_rpaf: Error parsing IP ", proxy_ip, " in ",
                               cmd->cmd->name, ". ", msgbuf, NULL);
        }
    }
    else
    {
      return apr_pstrcat(cmd->pool, "mod_rpaf: Error parsing IP \"", proxy_ip, "\" in ",
                         cmd->cmd->name, ". Failed basic parsing.", NULL);     
    }

    return NULL;
}

static const char *rpaf_set_headername(cmd_parms *cmd, void *dummy, const char *headername) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config,
                                                                   &rpaf_module);

    cfg->headername = headername;
    return NULL;
}

static const char *rpaf_enable(cmd_parms *cmd, void *dummy, int flag) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config,
                                                                   &rpaf_module);

    cfg->enable = flag;
    return NULL;
}

static const char *rpaf_set_forbid_if_not_proxy(cmd_parms *cmd, void *dummy, int flag) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config,
                                                                   &rpaf_module);

    cfg->forbid_if_not_proxy = flag;
    return NULL;
}

static const char *rpaf_sethostname(cmd_parms *cmd, void *dummy, int flag) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config,
                                                                   &rpaf_module);

    cfg->sethostname = flag;
    return NULL;
}

static const char *rpaf_sethttps(cmd_parms *cmd, void *dummy, int flag) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config,
                                                                   &rpaf_module);

    cfg->sethttps = flag;
    return NULL;
}

static const char *rpaf_setport(cmd_parms *cmd, void *dummy, int flag) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config,
                                                                   &rpaf_module);

    cfg->setport = flag;
    return NULL;
}

static int is_in_array(apr_sockaddr_t *remote_addr, apr_array_header_t *proxy_ips) {
    int i;
    apr_ipsubnet_t **subs = (apr_ipsubnet_t **)proxy_ips->elts;

    for (i = 0; i < proxy_ips->nelts; i++) {
        if (apr_ipsubnet_test(subs[i], remote_addr)) {
            return 1;
        }
    }

    return 0;
}

static apr_status_t rpaf_cleanup(void *data) {
    rpaf_cleanup_rec *rcr = (rpaf_cleanup_rec *)data;
    rcr->r->DEF_IP = apr_pstrdup(rcr->r->connection->pool, rcr->old_ip);
    rcr->r->DEF_ADDR->sa.sin.sin_addr.s_addr = apr_inet_addr(rcr->r->DEF_IP);
    return APR_SUCCESS;
}

static char *last_not_in_array(request_rec *r, apr_array_header_t *forwarded_for,
                               apr_array_header_t *proxy_ips) {
    apr_sockaddr_t *sa;
    apr_status_t rv;
    char **fwd_ips, *proxy_list;
    int i, earliest_legit_i = 0;

    proxy_list = apr_pstrdup(r->pool, r->DEF_IP);
    fwd_ips = (char **)forwarded_for->elts;

    for (i = (forwarded_for->nelts); i > 0; ) {
        i--;
        rv = apr_sockaddr_info_get(&sa, fwd_ips[i], APR_UNSPEC, 0, 0, r->pool);
        if (rv == APR_SUCCESS) {
            earliest_legit_i = i;
            if (!is_in_array(sa, proxy_ips))
                break;

            proxy_list = apr_pstrcat(r->pool, proxy_list, ", ", fwd_ips[i], NULL);
        }
        else {
            ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, 0, r,
                          "mod_rpaf: forwarded-for list entry of %s is not a valid IP", fwd_ips[i]);
        }
    }

    if (i > 0 || rv == APR_SUCCESS || earliest_legit_i) {
        /* remoteip-proxy-ip_list r->notes entry is forward compatible with Apache2.4 mod_remoteip*/
        apr_table_set(r->notes, "remoteip-proxy-ip-list", proxy_list);
        return fwd_ips[earliest_legit_i];
    }
    else {
        return NULL;
    }
}

static int rpaf_post_read_request(request_rec *r) {
    char *fwdvalue, *val, *mask, *last_val;
    int i;
    apr_port_t tmpport;
    apr_pool_t *tmppool;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(r->server->module_config,
                                                                   &rpaf_module);

    if (!cfg->enable)
        return DECLINED;

    /* this overcomes an issue when mod_rewrite causes this to get called again
       and the environment value is lost for HTTPS. This is the only thing that
       is lost and we do not need to process any further after restoring the
       value. */
    const char *rpaf_https = apr_table_get(r->connection->notes, "rpaf_https");
    if (rpaf_https) {
        apr_table_set(r->subprocess_env, "HTTPS", rpaf_https);
        return DECLINED;
    }

    /* check if the remote_addr is in the allowed proxy IP list */
    if (is_in_array(r->DEF_ADDR, cfg->proxy_ips) != 1) {
        if (cfg->forbid_if_not_proxy)
            return HTTP_FORBIDDEN;
        return DECLINED;
    }

    /* check if cfg->headername is set and if it is use
       that instead of X-Forwarded-For by default */
    if (cfg->headername && (fwdvalue = (char *)apr_table_get(r->headers_in, cfg->headername))) {
        //
    } else if (cfg->headername == NULL && (fwdvalue = (char *)apr_table_get(r->headers_in, "X-Forwarded-For"))) {
        //
    } else {
        return DECLINED;
    }

    /* if there was no forwarded for header then we dont do anything */
    if (!fwdvalue)
        return DECLINED;

    apr_array_header_t *arr = apr_array_make(r->pool, 4, sizeof(char *));

    while ((val = strsep(&fwdvalue, ",")) != NULL) {
        /* strip leading and trailing whitespace */
        while(isspace(*val))
            ++val;
        for (i = strlen(val) - 1; i > 0 && isspace(val[i]); i--)
            val[i] = '\0';
        if (rpaf_looks_like_ip(val))
            *(char **)apr_array_push(arr) = apr_pstrdup(r->pool, val);
    }

    if (arr->nelts == 0)
        return DECLINED;

    if ((last_val = last_not_in_array(r, arr, cfg->proxy_ips)) == NULL)
        return DECLINED;

    rpaf_cleanup_rec *rcr = (rpaf_cleanup_rec *)apr_pcalloc(r->pool, sizeof(rpaf_cleanup_rec));
    rcr->old_ip = apr_pstrdup(r->DEF_POOL, r->DEF_IP);
    rcr->r = r;
    apr_pool_cleanup_register(r->pool, (void *)rcr, rpaf_cleanup, apr_pool_cleanup_null);
    r->DEF_IP = apr_pstrdup(r->DEF_POOL, last_val);

    tmppool = r->DEF_ADDR->pool;
    tmpport = r->DEF_ADDR->port;
    apr_sockaddr_t *tmpsa;
    int ret = apr_sockaddr_info_get(&tmpsa, r->DEF_IP, APR_UNSPEC, tmpport, 0, tmppool);
    if (ret == APR_SUCCESS)
        memcpy(r->DEF_ADDR, tmpsa, sizeof(apr_sockaddr_t));
    if (cfg->sethostname) {
        const char *hostvalue;
        if ((hostvalue = apr_table_get(r->headers_in, "X-Forwarded-Host")) ||
            (hostvalue = apr_table_get(r->headers_in, "X-Host"))) {
            apr_array_header_t *arr = apr_array_make(r->pool, 0, sizeof(char*));
            while (*hostvalue && (val = ap_get_token(r->pool, &hostvalue, 1))) {
                *(char **)apr_array_push(arr) = apr_pstrdup(r->pool, val);
                if (*hostvalue != '\0')
                  ++hostvalue;
            }

            apr_table_set(r->headers_in, "Host", apr_pstrdup(r->pool, ((char **)arr->elts)[((arr->nelts)-1)]));
            r->hostname = apr_pstrdup(r->pool, ((char **)arr->elts)[((arr->nelts)-1)]);
            ap_update_vhost_from_headers(r);
        }
    }

    if (cfg->sethttps) {
        const char *httpsvalue, *scheme;
        if ((httpsvalue = apr_table_get(r->headers_in, "X-Forwarded-HTTPS")) ||
            (httpsvalue = apr_table_get(r->headers_in, "X-HTTPS"))) {
            apr_table_set(r->connection->notes, "rpaf_https", httpsvalue);
            apr_table_set(r->subprocess_env   , "HTTPS"     , httpsvalue);

            scheme = cfg->https_scheme;
        } else if ((httpsvalue = apr_table_get(r->headers_in, "X-Forwarded-Proto"))
                   && (strcmp(httpsvalue, cfg->https_scheme) == 0)) {
            apr_table_set(r->connection->notes, "rpaf_https", "on");
            apr_table_set(r->subprocess_env   , "HTTPS"     , "on");
            scheme = cfg->https_scheme;
        } else {
            scheme = cfg->orig_scheme;
        }
        #if AP_SERVER_MINORVERSION_NUMBER > 1 && AP_SERVER_PATCHLEVEL_NUMBER > 2
        r->server->server_scheme = scheme;
        #endif
    }

     if (cfg->setport) {
        const char *portvalue;
        if ((portvalue = apr_table_get(r->headers_in, "X-Forwarded-Port")) ||
            (portvalue = apr_table_get(r->headers_in, "X-Port"))) {
            r->server->port    = atoi(portvalue);
            r->parsed_uri.port = r->server->port;
        } else {
            r->server->port = cfg->orig_port;
        }
    }

    return DECLINED;
}

static const command_rec rpaf_cmds[] = {
    AP_INIT_FLAG(
                 "RPAF_Enable",
                 rpaf_enable,
                 NULL,
                 RSRC_CONF,
                 "Enable mod_rpaf"
                 ),
    AP_INIT_FLAG(
                 "RPAF_SetHostName",
                 rpaf_sethostname,
                 NULL,
                 RSRC_CONF,
                 "Let mod_rpaf set the hostname from the X-Host header and update vhosts"
                 ),
    AP_INIT_FLAG(
                 "RPAF_SetHTTPS",
                 rpaf_sethttps,
                 NULL,
                 RSRC_CONF,
                 "Let mod_rpaf set the HTTPS environment variable from the X-HTTPS header"
                 ),
    AP_INIT_FLAG(
                 "RPAF_SetPort",
                 rpaf_setport,
                 NULL,
                 RSRC_CONF,
                 "Let mod_rpaf set the server port from the X-Port header"
                 ),
    AP_INIT_FLAG(
                 "RPAF_ForbidIfNotProxy",
                 rpaf_set_forbid_if_not_proxy,
                 NULL,
                 RSRC_CONF,
                 "Deny access if connection not from trusted RPAF_ProxyIPs"
                 ),
    AP_INIT_ITERATE(
                 "RPAF_ProxyIPs",
                 rpaf_set_proxy_ip,
                 NULL,
                 RSRC_CONF,
                 "IP(s) of Proxy server setting X-Forwarded-For header"
                 ),
    AP_INIT_TAKE1(
                 "RPAF_Header",
                 rpaf_set_headername,
                 NULL,
                 RSRC_CONF,
                 "Which header to look for when trying to find the real ip of the client in a proxy setup"
                 ),
    { NULL }
};

static int ssl_is_https(conn_rec *c) {
    return apr_table_get(c->notes, "rpaf_https") != NULL;
}

static void rpaf_register_hooks(apr_pool_t *p) {
    ap_hook_post_read_request(rpaf_post_read_request, NULL, NULL, APR_HOOK_FIRST);

    /* this will only work if mod_ssl is not loaded */
    if (APR_RETRIEVE_OPTIONAL_FN(ssl_is_https) == NULL)
        APR_REGISTER_OPTIONAL_FN(ssl_is_https);
}

module AP_MODULE_DECLARE_DATA rpaf_module = {
    STANDARD20_MODULE_STUFF,
    NULL,
    NULL,
    rpaf_create_server_cfg,
    NULL,
    rpaf_cmds,
    rpaf_register_hooks,
};
