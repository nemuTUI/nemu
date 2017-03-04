#!/bin/sh

if [ -z "$1" ]; then
    echo "Usage: $0 <database_path>" >&2
    exit 1
fi

DB_PATH="$1"
DB_VERSION=$(sqlite3 "$DB_PATH" -line 'PRAGMA user_version;' | sed 's/.*\s=\s//')
RC=0

echo "database version: ${DB_VERSION}"

case "$DB_VERSION" in
    ( 0 )
        (
        sqlite3 "$DB_PATH" -line 'ALTER TABLE vms ADD mouse_override_1 integer;' &&
        sqlite3 "$DB_PATH" -line 'UPDATE vms SET mouse_override_1 = 42 WHERE mouse_override IS NULL;' &&
        sqlite3 "$DB_PATH" -line 'PRAGMA user_version=1'
        ) || RC=1
        ;;

    ( * )
        echo "Unsupported database user_version" >&2
        exit 1
esac

[ "$RC" = 0 ] && echo "done"
