#!/bin/bash
set -e

docker build -t gba-dev .

docker run --rm \
    -v "$(pwd)":/project \
    -w /project \
    gba-dev \
    make
