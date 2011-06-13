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

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_vhost.h"
#include "apr_strings.h"

#include <arpa/inet.h>

module AP_MODULE_DECLARE_DATA rpaf_module;

typedef struct {
    int                enable;
    int                sethostname;
    int                sethttps;
    int                setport;
    const char         *headername;
    apr_array_header_t *proxy_ips;
} rpaf_server_cfg;

typedef struct {
    const char  *old_ip;
    request_rec *r;
} rpaf_cleanup_rec;

static void *rpaf_create_server_cfg(apr_pool_t *p, server_rec *s) {
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)apr_pcalloc(p, sizeof(rpaf_server_cfg));
    if (!cfg)
        return NULL;

    cfg->proxy_ips = apr_array_make(p, 0, sizeof(char *));
    cfg->enable = 0;
    cfg->sethostname = 0;

    return (void *)cfg;
}

static const char *rpaf_set_proxy_ip(cmd_parms *cmd, void *dummy, const char *proxy_ip) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config, 
                                                                   &rpaf_module);

    /* check for valid syntax of ip */
    *(char **)apr_array_push(cfg->proxy_ips) = apr_pstrdup(cmd->pool, proxy_ip);
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

static int check_cidr(apr_pool_t *pool, const char *ipcidr, const char *testip) {
    char *ip;
    int ipcidr_len;
    int cidr_val;
    unsigned int netmask;
    char *cidr;
    /* TODO: this might not be portable.  just use struct in_addr instead? */
    uint32_t ipval, testipval;

    ip = apr_pstrdup(pool, ipcidr);
    /* TODO: this iterates once to copy and iterates again for length */
    ipcidr_len = strlen(ip);
    cidr = (char *) memchr(ip + (ipcidr_len - 3), '/', 3);

    if (cidr == NULL) {
        return -1;
    }

    *cidr = '\0';
    cidr++;
    cidr_val = atoi(cidr);
    if (cidr_val < 1 || cidr_val > 32) {
        return -1;
    }

    netmask = 0xffffffff << (32 - atoi(cidr));
    ipval = ntohl(inet_addr(ip));
    testipval = ntohl(inet_addr(testip));

    return (ipval & netmask) == (testipval & netmask);
}

static int is_in_array(apr_pool_t *pool, const char *remote_ip, apr_array_header_t *proxy_ips) {
    int i;
    char **list = (char**)proxy_ips->elts;
    for (i = 0; i < proxy_ips->nelts; i++) {
        if (check_cidr(pool, list[i], remote_ip) == 1) {
            return 1;
        }

        if (strcmp(remote_ip, list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static apr_status_t rpaf_cleanup(void *data) {
    rpaf_cleanup_rec *rcr = (rpaf_cleanup_rec *)data;
    rcr->r->connection->remote_ip   = apr_pstrdup(rcr->r->connection->pool, rcr->old_ip);
    rcr->r->connection->remote_addr->sa.sin.sin_addr.s_addr = apr_inet_addr(rcr->r->connection->remote_ip);
    return APR_SUCCESS;
}

static char* last_not_in_array(apr_pool_t *pool,
                               apr_array_header_t *forwarded_for,
                               apr_array_header_t *proxy_ips) {
    int i;
    for (i = (forwarded_for->nelts)-1; i > 0; i--) {
        if (!is_in_array(pool, ((char **)forwarded_for->elts)[i], proxy_ips))
           break;
    }
    return ((char **)forwarded_for->elts)[i];
}

static int change_remote_ip(request_rec *r) {
    const char *fwdvalue;
    char *val;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(r->server->module_config,
                                                                   &rpaf_module);

    if (!cfg->enable)
        return DECLINED;

    if (is_in_array(r->pool, r->connection->remote_ip, cfg->proxy_ips) == 1) {
        /* check if cfg->headername is set and if it is use
           that instead of X-Forwarded-For by default */
        if (cfg->headername && (fwdvalue = apr_table_get(r->headers_in, cfg->headername))) {
            //
        } else if ((fwdvalue = apr_table_get(r->headers_in, "X-Forwarded-For"))) {
            //
        } else {
            return DECLINED;
        }

        if (fwdvalue) {
            rpaf_cleanup_rec *rcr = (rpaf_cleanup_rec *)apr_pcalloc(r->pool, sizeof(rpaf_cleanup_rec));
            apr_array_header_t *arr = apr_array_make(r->pool, 0, sizeof(char*));
            while (*fwdvalue && (val = ap_get_token(r->pool, &fwdvalue, 1))) {
                *(char **)apr_array_push(arr) = apr_pstrdup(r->pool, val);
                if (*fwdvalue != '\0')
                    ++fwdvalue;
            }
            rcr->old_ip = apr_pstrdup(r->connection->pool, r->connection->remote_ip);
            rcr->r = r;
            apr_pool_cleanup_register(r->pool, (void *)rcr, rpaf_cleanup, apr_pool_cleanup_null);
            r->connection->remote_ip = apr_pstrdup(r->connection->pool, last_not_in_array(r->pool, arr, cfg->proxy_ips));
            r->connection->remote_addr->sa.sin.sin_addr.s_addr = apr_inet_addr(r->connection->remote_ip);

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
                const char *httpsvalue;
                if ((httpsvalue = apr_table_get(r->headers_in, "X-Forwarded-HTTPS")) ||
                    (httpsvalue = apr_table_get(r->headers_in, "X-HTTPS"))) {
                    apr_table_set(r->subprocess_env, "HTTPS", apr_pstrdup(r->pool, httpsvalue));
                }
            }

             if (cfg->setport) {
                const char *portvalue;
                if ((portvalue = apr_table_get(r->headers_in, "X-Forwarded-Port")) ||
                    (portvalue = apr_table_get(r->headers_in, "X-Port"))) {
                  r->server->port = atoi(portvalue);
                }
            }
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

static void register_hooks(apr_pool_t *p) {
    ap_hook_post_read_request(change_remote_ip, NULL, NULL, APR_HOOK_FIRST);
}

module AP_MODULE_DECLARE_DATA rpaf_module = {
    STANDARD20_MODULE_STUFF,
    NULL,
    NULL,
    rpaf_create_server_cfg,
    NULL,
    rpaf_cmds,
    register_hooks,
};
