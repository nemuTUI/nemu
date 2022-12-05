#compdef nemu

local -i rc=1

get_all_vms()
{
  nemu -l | awk -F ' - ' '{ printf "%s\n", $1}'
}

get_running_vms()
{
  nemu -l | awk -F ' - ' '{ if($2 == "running") printf "%s\n", $1}'
}

get_stopped_vms()
{
  nemu -l | awk -F ' - ' '{ if($2 == "stopped") printf "%s\n", $1}'
}

_nemu()
{
  local -a options=(
    {-v,--version}'[show version]'
    {-c,--create-veth}'[create veth interfaces]'
    {-d,--daemon}'[vm monitoring daemon]'
    {-l,--list}'[list vms]'
    {-h,--help}'[show help]'
    {-i,--info}+'[print vm info]: :->info'
    {-m,--cmd}+'[print vm command line]: :->cmd'
    {-p,--powerdown}+'[powerdown vm]: :->powerdown'
    {-f,--force-stop}+'[shutdown vm]: :->shutdown'
    {-z,--reset}+'[reset vm]: :->reset'
    {-k,--kill}+'[kill vm process]: :->kill'
    {-s,--start}+'[start vm]: :->start'
    {-C,--cfg}+'[path to config file]:cfg:_files'
    --snap-list+'[show snapshots]: :->snap-list'
    --snap-del+'[delete snapshot]: :->snap-del'
    --snap-save+'[create snapshot]: :->snap-save'
    --snap-load+'[load snapshot]: :->snap-load'
    --name+'[snapshot name]: :->snap-name'
  )

  _arguments -S -s : $options && rc=0

  case $state in
    (snap-name)
      if [ ${#words[@]} -ge 3 ]; then
        if [ $words[2] = "--snap-load" -o $words[2] = "--snap-del" ]; then
          local -a sub=($(nemu --snap-list $words[3] | \
            sed -E 's/^(.*)\s\[[0-9]{4}(-[0-9]{2}){2}\s([0-9]{2}:){2}[0-9]{2}\]$/\1/'))
          if [ ${#sub[@]} -gt 0 ]; then
            _values 'name' $sub
          fi
        fi
      fi
      rc=0
      ;;
    (info|cmd|snap-list|snap-del|snap-load)
      local -a sub=($(get_all_vms))
      if [ ${#sub[@]} -gt 0 ]; then
        _values 'val' $sub
      fi
      rc=0
      ;;
    (reset|kill|shutdown|powerdown|snap-save)
      local -a sub=($(get_running_vms))
      if [ ${#sub[@]} -gt 0 ]; then
        _values 'val' $sub
      fi
      rc=0
      ;;
    (start)
      local -a sub=($(get_stopped_vms))
      if [ ${#sub[@]} -gt 0 ]; then
        _values 'val' $sub
      fi
      rc=0
      ;;
  esac

  return $rc
}

_nemu "$@"
# vim: ft=zsh sw=2 sts=2 et
