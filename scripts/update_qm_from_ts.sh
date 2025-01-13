#!/bin/sh

here=$(dirname $0)

find $here/../translations -name "*.ts" -exec sh -c 'lconvert "$1" -o "${1%.ts}.qm"' _ {} \;

