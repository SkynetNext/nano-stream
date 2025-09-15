#include "UnityExport.h"
#include "UnityBattleContext.h"

#include <cstring>
#include <memory>
#include <string>

#include "DebugDraw.h"
#include "GameConfigMgr.h"
#include "JsonLoader.h"
#include "Logger.h"
#include "Version.h"

namespace {
std::string g_lastError;
int g_lastErrorCode = HGAME_BATTLE_SUCCESS;

void SetLastError(const std::string& error, int errorCode = HGAME_BATTLE_ERROR_INIT_FAILED) {
    g_lastError = error;
    g_lastErrorCode = errorCode;
    LOG_ERROR("UnityExport", "{}", error);
}

void ClearLastError() {
    g_lastError.clear();
    g_lastErrorCode = HGAME_BATTLE_SUCCESS;
}

// 验证上下文有效性
bool ValidateContext(HGameBattleContext* context) {
    if (!context || !context->handle) {
        SetLastError("Invalid context or handle", HGAME_BATTLE_ERROR_INVALID_PARAM);
        return false;
    }
    return true;
}

// 获取UnityBattleContext指针
UnityBattleContext* GetBattleContext(HGameBattleContext* context) {
    return static_cast<UnityBattleContext*>(context->handle);
}

}  // namespace

extern "C" {

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_Initialize(HGameBattleContext* context,
                       const char* battleEnterInfo,
                       int dataSize,
                       bool isServer) {
    if (!context) {
        LOG_ERROR("UnityExport", "context empty");
        SetLastError("Invalid context parameter", HGAME_BATTLE_ERROR_INVALID_PARAM);
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    try {
        ClearLastError();

        auto battleContext = std::make_unique<UnityBattleContext>();

        if (!battleContext->Initialize(battleEnterInfo, dataSize, isServer)) {
            SetLastError("Failed to initialize battle context", HGAME_BATTLE_ERROR_INIT_FAILED);
            return HGAME_BATTLE_ERROR_INIT_FAILED;
        }

        // 将上下文存储到句柄中
        context->handle = battleContext.release();
        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during initialization: ") + e.what(),
                     HGAME_BATTLE_ERROR_INIT_FAILED);
        return HGAME_BATTLE_ERROR_INIT_FAILED;
    } catch (...) {
        SetLastError("Unknown exception during initialization", HGAME_BATTLE_ERROR_INIT_FAILED);
        return HGAME_BATTLE_ERROR_INIT_FAILED;
    }
}

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_Shutdown(HGameBattleContext* context) {
    if (!ValidateContext(context)) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    try {
        ClearLastError();

        auto* battleContext = GetBattleContext(context);
        battleContext->Cleanup();
        delete battleContext;
        context->handle = nullptr;

        LOG_INFO("UnityExport", "Battle logic shutdown successfully");
        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during shutdown: ") + e.what(),
                     HGAME_BATTLE_ERROR_INIT_FAILED);
        return HGAME_BATTLE_ERROR_INIT_FAILED;
    } catch (...) {
        SetLastError("Unknown exception during shutdown", HGAME_BATTLE_ERROR_INIT_FAILED);
        return HGAME_BATTLE_ERROR_INIT_FAILED;
    }
}

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL HGameBattle_Update(HGameBattleContext* context,
                                                                        float deltaTime) {
    if (!ValidateContext(context)) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    if (deltaTime < 0.0f) {
        SetLastError("Invalid delta time", HGAME_BATTLE_ERROR_INVALID_PARAM);
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    try {
        ClearLastError();

        auto* battleContext = GetBattleContext(context);
        battleContext->Update(deltaTime);

        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during update: ") + e.what(),
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    } catch (...) {
        SetLastError("Unknown exception during update", HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    }
}

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_GetBufferInfo(HGameBattleContext* context, HGameBattleBufferInfo* info) {
    if (!ValidateContext(context) || !info) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    try {
        ClearLastError();

        auto* battleContext = GetBattleContext(context);
        auto bufferInfo = battleContext->GetBufferInfo();

        info->connected = bufferInfo.connected ? 1 : 0;
        info->totalSize = bufferInfo.total_size;
        info->inputWriteFrames = bufferInfo.input_stats.total_writes;
        info->inputReadFrames = bufferInfo.input_stats.total_reads;
        info->outputWriteFrames = bufferInfo.output_stats.total_writes;
        info->outputReadFrames = bufferInfo.output_stats.total_reads;

        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception getting buffer info: ") + e.what(),
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    } catch (...) {
        SetLastError("Unknown exception getting buffer info", HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    }
}

HGAME_BATTLE_API const char* HGAME_BATTLE_CALL HGameBattle_GetVersion() {
    return CPP_VERSION.c_str();
}

HGAME_BATTLE_API int HGAME_BATTLE_CALL HGameBattle_GetLastErrorCode() {
    return g_lastErrorCode;
}

HGAME_BATTLE_API const char* HGAME_BATTLE_CALL HGameBattle_GetLastErrorMessage() {
    return g_lastError.c_str();
}

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_CheckStatus(HGameBattleContext* context) {
    if (!ValidateContext(context)) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    try {
        ClearLastError();

        auto* battleContext = GetBattleContext(context);
        if (!battleContext) {
            SetLastError("Battle context is null", HGAME_BATTLE_ERROR_NOT_INITIALIZED);
            return HGAME_BATTLE_ERROR_NOT_INITIALIZED;
        }

        // 检查缓冲区连接状态
        auto bufferInfo = battleContext->GetBufferInfo();
        if (!bufferInfo.connected) {
            SetLastError("Buffer not connected", HGAME_BATTLE_ERROR_NOT_INITIALIZED);
            return HGAME_BATTLE_ERROR_NOT_INITIALIZED;
        }

        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during check status: ") + e.what(),
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    } catch (...) {
        SetLastError("Unknown exception during check status", HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    }
}

// ===== 配置管理API实现 =====

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL HGameBattle_LoadConfig(const char* configName,
                                                                            const char* configData,
                                                                            int dataSize,
                                                                            int startIndex,
                                                                            int endIndex) {
    if (!configName || !configData || dataSize <= 0) {
        SetLastError("Invalid parameters: configName, configData cannot be null, dataSize "
                     "must be positive",
                     HGAME_BATTLE_ERROR_INVALID_PARAM);
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    try {
        ClearLastError();

        // 直接使用const char*，减少字符串构造开销
        if (configName == nullptr) {
            SetLastError("Config name is null", HGAME_BATTLE_ERROR_INVALID_PARAM);
            return HGAME_BATTLE_ERROR_INVALID_PARAM;
        }

        if (configData == nullptr || dataSize <= 0) {
            SetLastError("Config data is null or size is invalid",
                         HGAME_BATTLE_ERROR_INVALID_PARAM);
            return HGAME_BATTLE_ERROR_INVALID_PARAM;
        }

        // 直接调用全局函数
        bool success = GameConfigMgr::LoadConfigFromBytes(
            configName, configData, dataSize, startIndex, endIndex);
        if (!success) {
            SetLastError("Failed to load config", HGAME_BATTLE_ERROR_UPDATE_FAILED);
            return HGAME_BATTLE_ERROR_UPDATE_FAILED;
        }

        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during load config: ") + e.what(),
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    } catch (...) {
        SetLastError("Unknown exception during load config", HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    }
}

HGAME_BATTLE_API const char* HGAME_BATTLE_CALL HGameBattle_GetTotalConfigHash() {
    try {
        ClearLastError();

        static std::string hashResult;  // 静态变量用于返回字符串
        hashResult = GameConfigMgr::getTotalConfigHash();

        LOG_INFO("UnityExport", "GetTotalConfigHash: {}", hashResult);

        return hashResult.c_str();

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during get total config hash: ") + e.what(),
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return "";
    } catch (...) {
        SetLastError("Unknown exception during get total config hash",
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return "";
    }
}

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL
HGameBattle_SetSkillJson(const char* skillName, const char* skillJson) {
    if (!skillName || !skillJson) {
        SetLastError("Invalid parameters: skillName, skillJson cannot be null",
                     HGAME_BATTLE_ERROR_INVALID_PARAM);
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    try {
        ClearLastError();

        // 按照StackOverflow最佳实践：直接使用const char*进行空指针检查
        if (skillName == nullptr) {
            SetLastError("Skill name is null", HGAME_BATTLE_ERROR_INVALID_PARAM);
            return HGAME_BATTLE_ERROR_INVALID_PARAM;
        }

        if (skillJson == nullptr) {
            SetLastError("Skill JSON is null", HGAME_BATTLE_ERROR_INVALID_PARAM);
            return HGAME_BATTLE_ERROR_INVALID_PARAM;
        }

        // 创建安全的std::string（只在内部使用）
        std::string skillNameStr(skillName);
        std::string skillJsonStr(skillJson);

        // 直接调用全局单例
        JsonLoader::getInstance().setSkillJson(skillNameStr, skillJsonStr);

        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during set skill JSON: ") + e.what(),
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    } catch (...) {
        SetLastError("Unknown exception during set skill JSON", HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    }
}

HGAME_BATTLE_API HGameBattleResult HGAME_BATTLE_CALL HGameBattle_SetDebugDrawEnabled(bool enabled) {
    try {
        ClearLastError();

        // 直接调用全局单例
        DebugDraw::getInstance().setEnabled(enabled);

        return HGAME_BATTLE_SUCCESS;

    } catch (const std::exception& e) {
        SetLastError(std::string("Exception during set debug draw enabled: ") + e.what(),
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    } catch (...) {
        SetLastError("Unknown exception during set debug draw enabled",
                     HGAME_BATTLE_ERROR_UPDATE_FAILED);
        return HGAME_BATTLE_ERROR_UPDATE_FAILED;
    }
}

}  // extern "C"

// 日志回调函数指针
static void (*g_logCallback)(const char* message, int level, int size) = nullptr;

// 内部日志函数实现
void SendLogToUnity(const char* message, int level) {
    if (g_logCallback != nullptr) {
        g_logCallback(message, level, static_cast<int>(strlen(message)));
    }
}

// 注册日志回调函数
extern "C" {
HGAME_BATTLE_API void HGAME_BATTLE_CALL
HGameBattle_RegisterLogCallback(void (*callback)(const char* message, int level, int size)) {
    g_logCallback = callback;
    if (callback != nullptr) {
        SendLogToUnity("C++ log callback registered successfully", 0);
    }
}

// ===== 直接内存写入API实现 =====

extern "C" HGameBattleResult HGameBattle_GetInputBufferState(HGameBattleContext* context,
                                                             void** bufferPtr,
                                                             size_t* capacity,
                                                             size_t* head,
                                                             size_t* tail) {
    if (!context || !bufferPtr || !capacity || !head || !tail) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    auto* battleContext = static_cast<UnityBattleContext*>(context->handle);
    if (!battleContext) {
        return HGAME_BATTLE_ERROR_NOT_INITIALIZED;
    }

    // 获取输入缓冲区的RingBuffer
    auto* ringBuffer = battleContext->GetInputBuffer();
    if (!ringBuffer) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    // 获取缓冲区状态
    *bufferPtr = ringBuffer->GetBufferAddress();
    *capacity = ringBuffer->GetCapacity();

    // 获取当前的head和tail
    auto result = ringBuffer->PrepareWrite(head, tail);
    if (result != RingBuffer::Error::kSuccess) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    return HGAME_BATTLE_SUCCESS;
}

extern "C" HGameBattleResult HGameBattle_CommitInputWrite(HGameBattleContext* context,
                                                          size_t newTail) {
    if (!context) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    auto* battleContext = static_cast<UnityBattleContext*>(context->handle);
    if (!battleContext) {
        return HGAME_BATTLE_ERROR_NOT_INITIALIZED;
    }

    // 获取输入缓冲区的RingBuffer
    auto* ringBuffer = battleContext->GetInputBuffer();
    if (!ringBuffer) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    // 获取当前的head，只更新tail
    size_t currentHead = ringBuffer->GetHead();

    // 提交写入（保持head不变，只更新tail）
    auto result = ringBuffer->CommitWrite(currentHead, newTail);
    if (result != RingBuffer::Error::kSuccess) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    return HGAME_BATTLE_SUCCESS;
}

extern "C" HGameBattleResult HGameBattle_GetOutputBufferState(HGameBattleContext* context,
                                                              void** bufferPtr,
                                                              size_t* capacity,
                                                              size_t* head,
                                                              size_t* tail) {
    if (!context || !bufferPtr || !capacity || !head || !tail) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    auto* battleContext = static_cast<UnityBattleContext*>(context->handle);
    if (!battleContext) {
        return HGAME_BATTLE_ERROR_NOT_INITIALIZED;
    }

    // 获取输出缓冲区的RingBuffer
    auto* ringBuffer = battleContext->GetOutputBuffer();
    if (!ringBuffer) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    // 获取缓冲区状态
    *bufferPtr = ringBuffer->GetBufferAddress();
    *capacity = ringBuffer->GetCapacity();

    // 获取当前的head和tail（对于读取，我们需要head和tail）
    *head = ringBuffer->GetHead();
    *tail = ringBuffer->GetTail();

    return HGAME_BATTLE_SUCCESS;
}

extern "C" HGameBattleResult HGameBattle_CommitOutputRead(HGameBattleContext* context,
                                                          size_t newHead) {
    if (!context) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    auto* battleContext = static_cast<UnityBattleContext*>(context->handle);
    if (!battleContext) {
        return HGAME_BATTLE_ERROR_NOT_INITIALIZED;
    }

    // 获取输出缓冲区的RingBuffer
    auto* ringBuffer = battleContext->GetOutputBuffer();
    if (!ringBuffer) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    // 提交读取（只更新head位置）
    auto result = ringBuffer->CommitRead(newHead);
    if (result != RingBuffer::Error::kSuccess) {
        return HGAME_BATTLE_ERROR_INVALID_PARAM;
    }

    return HGAME_BATTLE_SUCCESS;
}
}