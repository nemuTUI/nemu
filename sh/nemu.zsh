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
  )

  _arguments -S -s : $options && rc=0

  case $state in
    (info|cmd)
      local -a sub=($(get_all_vms))
      if [ ${#sub[@]} -gt 0 ]; then
        _values 'val' $sub && rc=0
      else
        rc=0
      fi
      ;;
    (reset|kill|shutdown|powerdown)
      local -a sub=($(get_running_vms))
      if [ ${#sub[@]} -gt 0 ]; then
        _values 'val' $sub && rc=0
      else
        rc=0
      fi
      ;;
    (start)
      local -a sub=($(get_stopped_vms))
      if [ ${#sub[@]} -gt 0 ]; then
        _values 'val' $sub && rc=0
      else
        rc=0
      fi
      ;;
  esac

  return $rc
}

_nemu "$@"
# vim: ft=zsh sw=2 sts=2 et
