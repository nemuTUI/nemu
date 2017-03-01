ALTER TABLE vms ADD mouse_override integer;
UPDATE vms SET mouse_override = 0 WHERE mouse_override IS NULL;
