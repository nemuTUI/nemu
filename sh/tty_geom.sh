#!/bin/bash

# Generate command for stty, this is usefull to fix tty geom on ntty serial connect 
stty -a | sed -r -e '/rows/!d' -e 's/^.*(rows\s[0-9]+);(\scolumns\s[0-9]+);.*$/stty \1\2/'
