#!/bin/sh
ssh root@robotlight.local "sudo mount -o remount,rw /"
scp tglight-test.toml root@robotlight.local:/opt/tglight/etc/tglight.toml
ssh root@robotlight.local "service tglight restart"

