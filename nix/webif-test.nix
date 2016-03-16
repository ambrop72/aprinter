/*
 * Copyright (c) 2015 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

{ writeText, writeScriptBin, lighttpd, haproxy, bash, ncd, aprinterWebif
, testHost ? "127.0.0.1"
, testPort ? 4012
, staticHost ? "127.0.0.1"
, staticPort ? 4003
, aprinterHost ? "192.168.111.142"
, aprinterPort ? 80
}:
let
    lighttpd_config = writeText "aprinter-webif-test-lighttpd.cfg" ''
        server.document-root = "${aprinterWebif}" 
        server.bind = "${staticHost}"
        server.port = ${toString staticPort}
        mimetype.assign = (
            ".htm"  => "text/html",
            ".html" => "text/html",
            ".css"  => "text/css",
            ".js"   => "application/javascript",
            ".png"  => "image/png",
            ".ico"  => "image/x-icon",
        )
        index-file.names = ( "reprap.htm" )
    '';
    
    haproxy_config = writeText "aprinter-webif-test-haproxy.cfg" ''
        defaults
            mode    http
            retries 3
            option  redispatch
            maxconn 32
            timeout connect     5s
            timeout client     50s
            timeout server    450s

        frontend http
            mode http
            bind ${testHost}:${toString testPort}
            acl rr_api path_beg -i /rr_
            use_backend device if rr_api
            default_backend lighty

        backend device
            option forwardfor
            server device1 ${aprinterHost}:${toString aprinterPort}

        backend lighty
            option forwardfor
            server lighty1 ${staticHost}:${toString staticPort}
    '';
    
    ncd_script = writeText "aprinter-webif-test.ncd" ''
        process main {
            daemon({"${lighttpd}/sbin/lighttpd", "-D", "-f", "${lighttpd_config}"}, ["keep_stderr": @true]);
            daemon({"${haproxy}/bin/haproxy", "-f", "${haproxy_config}"}, ["keep_stderr": @true]);
        }
    '';

in
writeScriptBin "aprinter-webif-test" ''
    #!${bash}/bin/bash
    exec -a badvpn-ncd ${ncd}/bin/badvpn-ncd --loglevel notice ${ncd_script}
''
