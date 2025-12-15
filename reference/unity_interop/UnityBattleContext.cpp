#include "UnityBattleContext.h"

#include <span>

#include "02401_02500_battle.pb.h"
#include "Battle.h"
#include "BattleMessageHandler.h"
#include "BattleUtils.h"
#include "BropokeBattle.h"
#include "ConfigHashCalculator.h"
#include "DebugDraw.h"
#include "EnvManager.h"
#include "GameConfigMgr.h"
#include "JsonLoader.h"
#include "Logger.h"
#include "MessageDispatcher.h"
#include "MessageFactory.h"

#include <algorithm>
#include <iostream>
#include <span>

UnityBattleContext::UnityBattleContext() = default;

UnityBattleContext::~UnityBattleContext() {
    Cleanup();
}

bool UnityBattleContext::Initialize(const char* battle_enter_info_str,
                                    int data_size,
                                    bool is_server) {
    if (initialized_) {
        LOG_WARN("UnityBattleContext", "Already initialized");
        return true;
    }

    try {
        // 测试Unity日志集成
        LOG_INFO("UnityBattleContext", "Starting initialize...");

        ::google::protobuf::BattleEnterInfo battle_enter_info_proto;
        if (!battle_enter_info_proto.ParseFromArray(battle_enter_info_str, data_size)) {
            LOG_ERROR("UnityBattleContext", "Failed to parse BattleEnterInfo");
            return false;
        }

        // 设置为客户端模式
        EnvManager::getInstance().setIsServer(is_server);

        // 初始化环形缓冲区
        input_buffer_ = std::make_unique<RingBuffer>();
        output_buffer_ = std::make_unique<RingBuffer>();

        auto enter_info = BattleUtils::createBattleEnterInfo(battle_enter_info_proto);
        if (!enter_info) {
            LOG_ERROR("UnityBattleContext", "Failed to create BattleEnterInfo");
            return false;
        }
        battle_ = std::make_unique<BropokeBattle>(enter_info);
        battle_->setActive(true);

        // 设置消息输出接口
        auto* message_handler = battle_->getMessageHandler();
        message_handler->setOutputBuffer(output_buffer_.get());

        initialized_ = true;
        LOG_INFO("UnityBattleContext",
                 "Battle context initialized: input buffer at {}, output buffer at {}",
                 GetInputBufferAddress(),
                 GetOutputBufferAddress());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("UnityBattleContext", "Exception during initialization: {}", e.what());
        Cleanup();
        return false;
    } catch (...) {
        LOG_ERROR("UnityBattleContext", "Unknown exception during initialization");
        Cleanup();
        return false;
    }
}

void UnityBattleContext::Cleanup() {
    if (!initialized_) {
        return;
    }

    // 清理Battle实例
    if (battle_) {
        battle_.reset();
    }

    // 清理缓冲区
    if (input_buffer_) {
        input_buffer_.reset();
    }
    if (output_buffer_) {
        output_buffer_.reset();
    }

    initialized_ = false;
    LOG_INFO("UnityBattleContext", "Battle context cleaned up");
}

void UnityBattleContext::Update(float delta_time) {
    if (!initialized_ || !battle_) {
        return;
    }

    // 处理输入消息
    ProcessIncomingMessages();

    // 更新战斗逻辑
    battle_->doUpdateWrapper();
}

void UnityBattleContext::Pause() {
    if (!initialized_ || !battle_) {
        return;
    }
    battle_->pause();
}

void UnityBattleContext::Resume() {
    if (!initialized_ || !battle_) {
        return;
    }
    battle_->resume();
}

UnityBattleContext::BufferInfo UnityBattleContext::GetBufferInfo() const {
    BufferInfo info;

    if (initialized_ && input_buffer_ && output_buffer_) {
        info.connected = true;
        info.total_size = RingBuffer::kDefaultCapacity * 2;  // 输入+输出
        info.input_stats = input_buffer_->GetStatistics();
        info.output_stats = output_buffer_->GetStatistics();
    }

    return info;
}

void UnityBattleContext::ProcessIncomingMessages() {
    if (!input_buffer_ || !battle_) {
        LOG_WARN("UnityBattleContext",
                 "ProcessIncomingMessages: input_buffer_={}, battle_={}",
                 static_cast<void*>(input_buffer_.get()),
                 static_cast<void*>(battle_.get()));
        return;
    }

    // 从输入缓冲区读取所有数据
    std::string data = input_buffer_->ReadAll();
    if (data.empty()) {
        return;  // 没有新数据
    }

    // 处理接收到的数据
    size_t offset = 0;
    const size_t dataSize = data.size();

    while (offset < dataSize) {
        // 检查是否有足够的数据读取消息头（4字节：2字节大小 + 2字节ID）
        if (offset + BattleMessageHandler::kMessageHeaderSize > dataSize) {
            LOG_WARN("UnityBattleContext", "Incomplete message header at offset {}", offset);
            break;
        }

        // 读取消息大小和ID
        auto dataSpan =
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());
        auto headerSpan = dataSpan.subspan(offset, BattleMessageHandler::kMessageHeaderSize);
        uint16_t messageSize = *reinterpret_cast<const uint16_t*>(headerSpan.data());
        uint16_t messageId =
            *reinterpret_cast<const uint16_t*>(headerSpan.subspan(sizeof(uint16_t)).data());

        // 检查消息大小是否合理
        if (messageSize == 0 || messageSize > 65535) {
            LOG_ERROR(
                "UnityBattleContext", "Invalid message size: {} at offset {}", messageSize, offset);
            break;
        }

        // 检查是否有足够的数据读取完整消息
        size_t totalMessageSize = BattleMessageHandler::kMessageHeaderSize + messageSize;
        if (offset + totalMessageSize > dataSize) {
            LOG_WARN("UnityBattleContext", "Incomplete message body at offset {}", offset);
            break;
        }

        // 提取消息体
        auto messageBodySpan =
            dataSpan.subspan(offset + BattleMessageHandler::kMessageHeaderSize, messageSize);
        std::string messageBody(reinterpret_cast<const char*>(messageBodySpan.data()), messageSize);

        // 解析并处理消息
        if (!parseAndDispatchMessage(messageId, messageBody)) {
            LOG_WARN("UnityBattleContext",
                     "Failed to parse message with ID: {} at offset {}",
                     messageId,
                     offset);
        }

        // 移动到下一个消息
        offset += totalMessageSize;
    }
}

bool UnityBattleContext::parseAndDispatchMessage(uint16_t messageId,
                                                 const std::string& messageBody) {
    if (!battle_) {
        LOG_ERROR("UnityBattleContext", "Battle instance not initialized");
        return false;
    }

    // 使用 MessageFactory 创建消息实例
    auto message = MessageFactory::getInstance().createMessageByMsgId(messageId);
    if (!message) {
        LOG_WARN("UnityBattleContext", "Unknown message ID: {}", messageId);
        return false;
    }

    // 反序列化消息体
    if (!message->ParseFromString(messageBody)) {
        LOG_ERROR("UnityBattleContext", "Failed to parse message body for ID: {}", messageId);
        return false;
    }

    // 分发消息到 Battle 系统
    // 使用 Battle 的 MessageDispatcher 分发消息
    auto* dispatcher = battle_->getMessageDispatcher();
    if (dispatcher) {
        dispatcher->dispatch(messageId, *message);
    } else {
        LOG_ERROR("UnityBattleContext", "MessageDispatcher not available in Battle");
        return false;
    }

    return true;
}

void* UnityBattleContext::GetInputBufferAddress() const {
    if (!input_buffer_) {
        return nullptr;
    }
    return input_buffer_->GetBufferAddress();
}

void* UnityBattleContext::GetOutputBufferAddress() const {
    if (!output_buffer_) {
        return nullptr;
    }
    return output_buffer_->GetBufferAddress();
}

size_t UnityBattleContext::GetBufferTotalSize() const {
    return RingBuffer::kDefaultCapacity;
}