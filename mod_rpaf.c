
/* ====================================================================
 * Copyright (c) 1995 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * IT'S CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/*
 * $Id: mod_rpaf.c 17 2008-01-01 03:03:15Z thomas $
 *
 * Author: Thomas Eibner, <thomas@stderr.net>
 * URL: http://stderr.net/apache/rpaf/
 * rpaf is short for reverse proxy add forward 
 *
 * This module does the opposite of mod_proxy_add_forward written by
 * Ask Bjørn Hansen. http://develooper.com/code/mpaf/ or mod_proxy
 * in 1.3.25 and above and mod_proxy from Apache 2.0
 * 
 */ 

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_vhost.h"

module MODULE_VAR_EXPORT rpaf_module;

typedef struct {
    int          enable;
    int          sethostname;
    const char   *headername;
    array_header *proxy_ips;
} rpaf_server_cfg;

typedef struct {
    const char  *old_ip;
    request_rec *r;
} rpaf_cleanup_rec;

static void *rpaf_create_server_cfg(pool *p, server_rec *s) {
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_pcalloc(p, sizeof(rpaf_server_cfg));
    if (!cfg)
        return NULL;

    cfg->proxy_ips = ap_make_array(p, 0, sizeof(char *));
    cfg->enable = 0;
    cfg->sethostname = 0;

    return (void *)cfg;
}

static const char *rpaf_set_proxy_ip(cmd_parms *cmd, void *dummy, char *proxy_ip) {
    server_rec *s = cmd->server;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(s->module_config, 
                                                                   &rpaf_module);

    /* check for valid syntax of ip */

    *(char **)ap_push_array(cfg->proxy_ips) = ap_pstrdup(cmd->pool, proxy_ip);
    return NULL;
}

static const char *rpaf_set_headername(cmd_parms *cmd, void *dummy, char *headername) {
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

static int is_in_array(const char *remote_ip, array_header *proxy_ips) {
    int i;
    char **list = (char**)proxy_ips->elts;
    for (i = 0; i < proxy_ips->nelts; i++) {
        if (strcmp(remote_ip, list[i]) == 0)
            return 1;
    }
    return 0;
}

static void rpaf_cleanup(void *data) {
    rpaf_cleanup_rec *rcr = (rpaf_cleanup_rec *)data;
    rcr->r->connection->remote_ip   = ap_pstrdup(rcr->r->connection->pool, rcr->old_ip);
    rcr->r->connection->remote_addr.sin_addr.s_addr = inet_addr(rcr->r->connection->remote_ip);
}

static int change_remote_ip(request_rec *r) {
    const char *fwdvalue;
    char *val;
    rpaf_server_cfg *cfg = (rpaf_server_cfg *)ap_get_module_config(r->server->module_config,
                                                                   &rpaf_module);

    if (!cfg->enable)
        return DECLINED;

    if (is_in_array(r->connection->remote_ip, cfg->proxy_ips) == 1) {
        /* check if cfg->headername is set and if it is use 
           that instead of X-Forwarded-For by default */
        if (cfg->headername && (fwdvalue = ap_table_get(r->headers_in, cfg->headername))) {
            //
        } else if (fwdvalue = ap_table_get(r->headers_in, "X-Forwarded-For")) {
            //
        } else { 
            return DECLINED;
        }

        if (fwdvalue) {
            rpaf_cleanup_rec *rcr = (rpaf_cleanup_rec *)ap_pcalloc(r->pool, sizeof(rpaf_cleanup_rec));
            array_header *arr = ap_make_array(r->pool, 0, sizeof(char*));
            while (*fwdvalue && (val = ap_get_token(r->pool, &fwdvalue, 1))) {
                *(char **)ap_push_array(arr) = ap_pstrdup(r->pool, val);
                if (*fwdvalue != '\0')
                    ++fwdvalue;
            }
            rcr->old_ip = ap_pstrdup(r->connection->pool, r->connection->remote_ip);
            rcr->r = r;
            ap_register_cleanup(r->pool, (void *)rcr, rpaf_cleanup, ap_null_cleanup);
            r->connection->remote_ip = ap_pstrdup(r->connection->pool, ((char **)arr->elts)[((arr->nelts)-1)]);
            r->connection->remote_addr.sin_addr.s_addr = inet_addr(r->connection->remote_ip);
            if (cfg->sethostname) {
                const char *hostvalue;
                if (hostvalue = ap_table_get(r->headers_in, "X-Forwarded-Host")) {
                    /* 2.0 proxy frontend or 1.3 => 1.3.25 proxy frontend */
                    ap_table_set(r->headers_in, "Host", ap_pstrdup(r->pool, hostvalue));
                    r->hostname = ap_pstrdup(r->pool, hostvalue);
                    ap_update_vhost_from_headers(r);
                } else if (hostvalue = ap_table_get(r->headers_in, "X-Host")) {
                    /* 1.3 proxy frontend with mod_proxy_add_forward */
                    ap_table_set(r->headers_in, "Host", ap_pstrdup(r->pool, hostvalue));
                    r->hostname = ap_pstrdup(r->pool, hostvalue);
                    ap_update_vhost_from_headers(r);
                }
            }
        }
    }
    return DECLINED;
}

static command_rec rpaf_cmds[] = {
    { "RPAFenable", rpaf_enable, NULL,
      RSRC_CONF, FLAG, "Enable mod_rpaf" },
    { "RPAFsethostname", rpaf_sethostname, NULL,
      RSRC_CONF, FLAG, "Let mod_rpaf set the hostname from the X-Host header and update vhosts" },
    { "RPAFproxy_ips", rpaf_set_proxy_ip, NULL,
      RSRC_CONF, ITERATE, "IP(s) of Proxy server setting X-Forwarded-For header" },
    { "RPAFheader", rpaf_set_headername, NULL,
      RSRC_CONF, TAKE1, "Which header to look for when trying to find the real ip of the client in a proxy setup" },
    { NULL }
};

module MODULE_VAR_EXPORT rpaf_module = {
    STANDARD_MODULE_STUFF,
    NULL,                              /* initializer */
    NULL,                              /* dir config creator */
    NULL,                              /* dir config merger */
    rpaf_create_server_cfg,            /* server config */
    NULL,                              /* merge server config */
    rpaf_cmds,                         /* command table */
    NULL,                              /* handlers */
    NULL,                              /* filename translation */
    NULL,                              /* check_user_id */
    NULL,                              /* check auth */
    NULL,                              /* check access */
    NULL,                              /* type_checker */
    NULL,                              /* fixups */
    NULL,                              /* logger */
    NULL,                              /* header parser */
    NULL,                              /* child_init */
    NULL,                              /* child_exit */
    change_remote_ip                   /* post read-request */
};

  
