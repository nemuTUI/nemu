#!/usr/bin/env bash

get_running_vms()
{
    nemu -l | awk 'BEGIN { FS = " - " }; { if($2 == "running") printf "%s\n", $1}'
}

get_stopped_vms()
{
    nemu -l | awk 'BEGIN { FS = " - " }; { if($2 == "stopped") printf "%s\n", $1}'
}

_nemu_completions()
{
    local curr prev

    curr=${COMP_WORDS[COMP_CWORD]}
    prev=${COMP_WORDS[COMP_CWORD-1]}

    COMPREPLY=()

    if [[ "$COMP_CWORD" == 1 ]]; then
        COMPREPLY=( $(compgen -W "-h --help -l --list -s --start -p --powerdown \
            -f --force-stop -z --reset -k --kill -v --version" -- "$curr") )
    elif [[ "$COMP_CWORD" == 2 ]]; then
        case "$prev" in
            "-s"|"--start")
                COMPREPLY=( $(compgen -W "$(get_stopped_vms)" -- "$curr") )
            ;;
            "-p"|"--powerdown"|"-f"|"--force-stop"|"-z"|"--reset"|"-k"|"--kill")
                COMPREPLY=( $(compgen -W "$(get_running_vms)" -- "$curr") )
            ;;
            *)
            ;;
        esac
    fi

    return 0
} &&

complete -F _nemu_completions nemu
