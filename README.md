## mod_rpaf - reverse proxy add forward

### Summary

Sets `REMOTE_ADDR`, `HTTPS`, and `HTTP_PORT` to the values provided by an upstream proxy.
Sets `remoteip-proxy-ip-list` field in r->notes table to list of proxy intermediaries.


### Compile Debian/Ubuntu Package and Install

    sudo apt-get install build-essential apache2-threaded-dev yada
    make
    make install   

### Compile and Install for RedHat/CentOS

    yum install httpd-devel
    make
    make install

### Configuration Directives

    RPAF_Enable             (On|Off)                - Enable reverse proxy add forward

    RPAF_ProxyIPs           127.0.0.1 10.0.0.0/24   - What IPs & bitmaksed subnets to adjust
                                                      requests for

    RPAF_Header             X-Forwarded-For         - The header to use for the real IP 
                                                      address.

    RPAF_SetHostName        (On|Off)                - Update vhost name so ServerName &
                                                      ServerAlias work

    RPAF_SetHTTPS           (On|Off)                - Set the HTTPS environment variable
                                                      to the header value contained in
                                                      X-HTTPS, or X-Forwarded-HTTPS. For
                                                      best results make sure that mod_ssl
                                                      is NOT enabled.

    RPAF_SetPort            (On|Off)                - Set the server port to the header
                                                      value contained in X-Port, or
                                                      X-Forwarded-Port. (See Issue #12)

    RPAF_ForbidIfNotProxy   (On|Off)                - Option to forbid request if not from
                                                      trusted RPAF_ProxyIPs; otherwise
                                                      cannot be done with Allow/Deny after
                                                      remote addr substitution
                                                      

## Example Configuration

    LoadModule              rpaf_module modules/mod_rpaf.so
    RPAF_Enable             On
    RPAF_ProxyIPs           127.0.0.1 10.0.0.0/24
    RPAF_SetHostName        On
    RPAF_SetHTTPS           On
    RPAF_SetPort            On
    RPAF_ForbidIfNotProxy   Off

## Authors

* Thomas Eibner <thomas@stderr.net>
* Geoffrey McRae <gnif@xbmc.org>
* Proxigence Inc. <support@proxigence.com>

## License and distribution

This software is licensed under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0). The
latest version is available [from GitHub](http://github.com/gnif/mod_rpaf)
