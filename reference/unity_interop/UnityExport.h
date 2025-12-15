#pragma once

#ifdef _WIN32
#ifdef HGAME_BATTLE_DLL_EXPORTS
#define HGAME_BATTLE_API __declspec(dllexport)
#else
#define HGAME_BATTLE_API __declspec(dllimport)
#endif
#define HGAME_BATTLE_CALL __stdcall
#else
#define HGAME_BATTLE_API __attribute__((visibility("default")))
#define HGAME_BATTLE_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 错误码定义
typedef enum {
    HGAME_BATTLE_SUCCESS = 0,
    HGAME_BATTLE_ERROR_INVALID_PARAM = -1,
    HGAME_BATTLE_ERROR_INIT_FAILED = -2,
    HGAME_BATTLE_ERROR_NOT_INITIALIZED = -3,
    HGAME_BATTLE_ERROR_UPDATE_FAILED = -4,
    HGAME_BATTLE_ERROR_PAUSE_FAILED = -5,
    HGAME_BATTLE_ERROR_RESUME_FAILED = -6,
} HGameBattleResult;

// 简化的上下文句柄
typedef struct {
    void* handle;  // 内部句柄，Unity端不需要关心具体内容
} HGameBattleContext;

// 新版环形缓冲区统计信息（建议改用此结构与接口）
typedef struct {
    int connected;
    size_t inputCapacity;
    size_t outputCapacity;
    uint64_t inputTotalWrites;
    uint64_t inputTotalReads;
    uint64_t inputFailedWrites;
    uint64_t inputFailedReads;
    uint64_t inputCurrentSize;
    uint64_t outputTotalWrites;
    uint64_t outputTotalReads;
    uint64_t outputFailedWrites;
    uint64_t outputFailedReads;
    uint64_t outputCurrentSize;
} HGameBattleBufferStats;

// ===== 零拷贝API =====

// 初始化战斗逻辑系统
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_Initialize(HGameBattleContext* context,
                       const char* battleEnterInfo,
                       int dataSize,
                       bool isServer);

// 清理战斗逻辑系统
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_Shutdown(HGameBattleContext* context);

// 处理一帧的逻辑更新（唯一的每帧调用）
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL HGameBattle_Update(HGameBattleContext* context,
                                                                        float deltaTime);

// 逻辑暂停
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL HGameBattle_Pause(HGameBattleContext* context);

// 逻辑运行
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_Resume(HGameBattleContext* context);

// 获取环形缓冲区统计信息
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_GetBufferInfo(HGameBattleContext* context, HGameBattleBufferStats* stats);

// ===== 信息查询API =====

// 获取版本信息
HGAME_BATTLE_API const char* HGAME_BATTLE_CALL HGameBattle_GetVersion();

// 获取最后错误码
HGAME_BATTLE_API int HGAME_BATTLE_CALL HGameBattle_GetLastErrorCode();

// 获取最后错误信息
HGAME_BATTLE_API const char* HGAME_BATTLE_CALL HGameBattle_GetLastErrorMessage();

// 检查系统状态
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_CheckStatus(HGameBattleContext* context);

// ===== 高性能批量通信API =====

// ===== 内存地址获取API =====

// ===== 直接内存写入API =====

// 获取输入缓冲区的当前状态（地址、容量、head、tail）
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_GetInputBufferState(HGameBattleContext* context,
                                void** bufferPtr,
                                size_t* capacity,
                                size_t* head,
                                size_t* tail);

// 提交写入（更新tail位置）
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_CommitInputWrite(HGameBattleContext* context, size_t newTail);

// 获取输出缓冲区的当前状态（地址、容量、head、tail）
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_GetOutputBufferState(HGameBattleContext* context,
                                 void** bufferPtr,
                                 size_t* capacity,
                                 size_t* head,
                                 size_t* tail);

// 提交读取（更新head位置）
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_CommitOutputRead(HGameBattleContext* context, size_t newHead);

// ===== 配置管理API =====

// 加载指定配置（使用const char*，传递二进制数据）
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL HGameBattle_LoadConfig(const char* configName,
                                                                            const char* configData,
                                                                            int dataSize,
                                                                            int startIndex,
                                                                            int endIndex);

// 获取总配置哈希值
HGAME_BATTLE_API const char* HGAME_BATTLE_CALL HGameBattle_GetTotalConfigHash();

// 设置技能JSON配置（使用const char*，兼容P/Invoke）
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_SetSkillJson(const char* skillName, const char* skillJson);

// 设置调试绘制开关
HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL HGameBattle_SetDebugDrawEnabled(bool enabled);

// 注册日志回调函数（用于动态加载模式下的日志输出）
HGAME_BATTLE_API void HGAME_BATTLE_CALL
HGameBattle_RegisterLogCallback(void (*callback)(const char* message, int level, int size));

#ifdef __cplusplus
}
#endif