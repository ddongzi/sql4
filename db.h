#ifndef DB_H
#define DB_H
#include "table.h"

/*  TODO 目前是单表，  db 实际上就是table*/
void db_close(Table *table);
Table *db_open(const char *file_name);

#endif