#!/bin/sh

process="$1"
user="$2"
group="$3"
signal="$4"
time="$5"
app="$6"

/usr/lib/systemd/systemd-coredump "$process" "$user" "$group" "$signal" "$time" "$app"

rootpath=/opt/usr/share/crash/dump
name="$app"_"$process"_"$time"
path="$rootpath"/"$name"
info="$path"/"$name".info
dump="$path"/"$name".coredump
log="$path"/"$name".log

/usr/bin/mkdir -p "$rootpath"
/usr/bin/mkdir -p "$path"

/usr/bin/coredumpctl dump "$process" --output="$dump"

/usr/bin/coredumpctl info "$process" >> "$info"
/usr/bin/coredumpctl dump "$process" --output="$dump"
/usr/bin/dump_systemstate -d -k -f "$log"
