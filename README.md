# draw
一个在线绘图软件
sudo apt-get install libfcgi-dev libjson-c-dev libmypaint-dev

10-fastcgi-draw.conf for lighttpd

# -*- depends: fastcgi -*-
# /usr/share/doc/lighttpd/fastcgi.txt.gz
# http://redmine.lighttpd.net/projects/lighttpd/wiki/Docs:ConfigurationOptions#mod_fastcgi-fastcgi

## Start FastCGI server for draw (needs the draw package)
fastcgi.server += (
	"/draw.fcgi" =>
	((
		"bin-path"     => "/usr/local/bin/draw.fcgi",
		"socket"       => "/run/lighttpd/draw.socket",
		"check-local"  => "disable",
		"max-procs"    => 1
	))
)

