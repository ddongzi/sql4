/**
 * @brief 所有的返回玛、错误玛
 */
#include <stdint.h>
uint16_t sql4_errno;    // 全局
typedef uint16_t SQL4_CODE;

#define EXECUTE_SUCCESS 0x0000
#define EXECUTE_TABLE_FULL 0x0001
#define EXECUTE_DUPLICATE_KEY 0x0002
#define EXECUTE_ERR 0x0003

#define BTREE_INSERT_SUCCESS 0x1000
#define BTREE_DUPLICATE_KEY 0x1001
#define BTREE_SELECT_SUCCESS 0x1002
#define BTREE_DELETE_SUCCESS 0x1002

#define COMPLIER_PARSE_SUCCESS 0X2000

#define VDBE_OK 0x3000