#ifndef fs
#define fs
#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26
#include <fuse.h>
//структуры по одному байту
#pragma pack(1)
#ifdef DEBUG
#define TRACE printf("[DEBUG] FILE:%s LINE:%d\n", __FILE__, __LINE__);
#else

#define TRACE
#endif

#define FILESYSTEM "filesystem"
#define NUMROOTBLOCK 0//номер корневого каталога
#define SIZE_BLOCK 4096
#define NODE_NAME_MAX_SIZE 256 //макс размер имени
#define BLOCK_STATUS_OFFSET 0
#define NODE_NAME_OFFSET (BLOCK_STATUS_OFFSET + sizeof(char))
#define NODE_STAT_OFFSET (NODE_NAME_OFFSET + NODE_NAME_MAX_SIZE)
#define NODE_CONTENT_OFFSET (NODE_STAT_OFFSET + sizeof(stat_file_t))
#define NODE_CONTENT_MAX_SIZE (SIZE_BLOCK - NODE_CONTENT_OFFSET)

//структура,хранящая информацию о файле
typedef struct stat stat_file_t;
//структура, определяющая файл или папка
typedef struct node inode_t;

enum boolean
{
    FALSE = 0,
    TRUE = 1,
};

struct node
{
    char status;
    char name[NODE_NAME_MAX_SIZE];
    stat_file_t stat;
    char content[0];
};

enum block_status
{
    BLOCK_IS_FREE = 0,
    BLOCK_IS_dir = 1,
    BLOCK_IS_FILE = 2,
};

//загружаем данные из файла и инициализация глоб переменных
int load();
//создаем корневую
int createRoot();
void *createBlock();
int createDir(const char *name, mode_t mode);
int createFile(const char *name, mode_t mode, dev_t dev);
char *createName(const char *name);
// создать пустое имя
char *createEmptyName();

/*парсер*/
char **parserPath(const char *path);
// исключить имя последнего узла
char *excludeLastNodeName(char **nodeNames);

int readBlock(int num, void *block);
int writeBlock(int num, void *block);

// удалить файл, метка свободный
int removeFile(int num);
// удалить папку
int removeDir(int num);
// удалить блок
int removeBlock(int num);
//  уничтожить блок
void freeMemoryBlock(void *block);
// освободить память
void freeMemoryName(char *name);
// освободить память
void freeMemoryNodeNames(char **nodeNames);
// стереть блок
int clearBlock(int num);
// удалить узел из папки
int removeNodeFromDir(int dir_num, int node_num);

// получить блок
void *getBlock(int num);
// получить состояние блока
int getBlockStatus(int num);
// получить имя узла
int getInodeName(int num, char *buf);
// получить атрибуты узла
int getInodeStat(int num, stat_file_t *stbuf);

// задать состояние блока
int setBlockStatus(int num, char status);
// задать имя узла
int setInodeName(int num, char *buf);
// задать атрибуты узла
int setInodeStat(int num, stat_file_t *buf);

// добавить узел в папку
int addInodeToDir(int dir_num, int node_num);

// искать первый свободный блок
int searchFreeBlock();
// найти узел
int searchInode(int node_num, char **nodeNames);
// поиск узла в папке
int searchInodeInDir(int dir_num, const char *node_name);

extern const int sizeBlock;
extern int fs_fd;
extern const int numRootBlock;

#endif