#!/usr/bin/sh
cpuraytracer --spp 128 -O -500.0f,200.0f,200.0f -T 0.0f,0.0f,0.0f -F 90.0f -f ./test-models/mcguire/bmw/bmw.obj $@
