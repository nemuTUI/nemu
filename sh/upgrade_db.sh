#!/bin/sh

if [ -z "$1" ]; then
    echo "Usage: $0 <database_path>" >&2
    exit 1
fi

DB_PATH="$1"
DB_ACTUAL_VERSION=2
DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*\s=\s//')
RC=0

update_ifs()
{
  local vm vms netjs
  vms=($(sqlite3 "$DB_PATH" -line 'SELECT name FROM vms;' | sed 's/.*\s=\s//'))

  for vm in "${vms[@]}"; do
    netjs=$(sqlite3 "$DB_PATH" -line "SELECT mac FROM vms WHERE name='${vm}';" | sed 's/.*\s=\s//') &&
    netjs=$(echo "$netjs" | sed -r 's/("drv":"[a-z0-9]+")/\1,"ip4":""/g') &&
    sqlite3 "$DB_PATH" -line "UPDATE vms SET mac = '${netjs}' WHERE name='${vm}';"
  done

  return 0
}

if [ "$DB_CURRENT_VERSION" = "$DB_ACTUAL_VERSION" ]; then
    echo "No need to upgrade."
    exit 0
fi

echo "database version: ${DB_CURRENT_VERSION}"
while [ "$DB_CURRENT_VERSION" != "$DB_ACTUAL_VERSION" ]; do
    [ "$RC" = 1 ] && break;

    case "$DB_CURRENT_VERSION" in
        ( 0 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD mouse_override integer;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET mouse_override = 0 WHERE mouse_override IS NULL;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=1'
            ) || RC=1
            ;;

        ( 1 )
            (
            update_ifs &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD drive_interface char;' &&
            sqlite3 "$DB_PATH" -line 'UPDATE vms SET drive_interface = "ide" WHERE drive_interface IS NULL;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD kernel_append char;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD tty_path char;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD socket_path char;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=2'
            ) || RC=1
            ;;

        ( * )
            echo "Unsupported database user_version" >&2
            exit 1
    esac
    
    DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*\s=\s//')
done

[ "$RC" = 0 ] && echo "database upgrade complete."
