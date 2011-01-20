## mod_rpaf - reverse proxy add forward

### Summary

Sets `REMOTE_ADDR`, `HTTPS`, and `HTTP_PORT` to the values provided by an upstream proxy.

### Compile Debian/Ubuntu Package and Install

    sudo apt-get install build-essential apache2-threaded-dev yada
    dpkg-buildpackage -b
    sudo dpkg -i ../libapache2-mod-rpaf_X.X-X.X_XXX.deb

### Compile and Install for RedHat/CentOS

    yum install httpd-devel
    make
    make install

### Configuration Directives

    RPAF_Enable      (On|Off)           - Enable reverse proxy add forward

    RPAF_ProxyIPs    127.0.0.1 10.0.0.1 - What IPs to adjust requests for

    RPAF_Header      X-Forwarded-For    - The header to use for the real IP
                                          address.

    RPAF_SetHostName (On|Off)           - Update vhost name so ServerName &
                                          ServerAlias work

    RPAF_SetHTTPS    (On|Off)           - Set the HTTPS environment variable
                                          to the header value contained in
                                          X-HTTPS, or X-Forwarded-HTTPS.

    RPAF_SetPort     (On|Off)           - Set the server port to the header
                                          value contained in X-Port, or
                                          X-Forwarded-Port.

## Example Configuration

    LoadModule        modules/mod_rpaf.so
    RPAF_Enable       On
    RPAF_ProxyIPs     127.0.0.1 10.0.0.10 10.0.0.20
    RPAF_SetHostName  On
    RPAF_SetHTTPS     On
    RPAF_SetPort      On
  
## Authors

* Thomas Eibner <thomas@stderr.net>
* Geoffrey McRae <gnif@xbmc.org>

## License and distribution

This software is licensed under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0). The
latest version is available [from GitHub](http://github.com/gnif/mod_rpaf)
