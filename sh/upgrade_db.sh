#!/bin/sh

if [ -z "$1" ]; then
    echo "Usage: $0 <database_path>" >&2
    exit 1
fi

DB_PATH="$1"
DB_ACTUAL_VERSION=4
DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*\s=\s//')
RC=0

if [ "$DB_CURRENT_VERSION" -lt 3 ]; then
    echo "Database version less then 3 is not supported"
    exit 1
fi

if [ "$DB_CURRENT_VERSION" = "$DB_ACTUAL_VERSION" ]; then
    echo "No need to upgrade."
    exit 0
fi

echo "database version: ${DB_CURRENT_VERSION}"
while [ "$DB_CURRENT_VERSION" != "$DB_ACTUAL_VERSION" ]; do
    [ "$RC" = 1 ] && break;

    case "$DB_CURRENT_VERSION" in
        ( 3 )
            (
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD initrd char;' &&
            sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD machine char;' &&
            sqlite3 "$DB_PATH" -line 'PRAGMA user_version=4'
            ) || RC=1
            ;;

        ( * )
            echo "Unsupported database user_version" >&2
            exit 1
    esac
    
    DB_CURRENT_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*\s=\s//')
done

[ "$RC" = 0 ] && echo "database upgrade complete."
