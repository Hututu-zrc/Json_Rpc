/*                      该
        类的主要作用就是规定后续代码要使用的字段和类型
*/
#pragma once
#include <string>
#include <unordered_map>

namespace zrcrpc
{

    // 这里定义常用字段的宏定义出来，需要改名字的时候，直接修改宏对应的就行，不需要一个一个修改
#define KEY_METHOD "method"
#define KEY_PARAMS "parameters"
#define KEY_TOPIC_KEY "topic_key"
#define KEY_TOPIC_MSG "topic_msg"
#define KEY_OPTYPE "optype"
#define KEY_HOST "host"
#define KEY_HOST_IP "ip"
#define KEY_HOST_PORT "port"
#define KEY_RCODE "rcode"
#define KEY_RESULT "result"

    // 消息的操作类型
    enum class MType
    {
        REQ_RPC = 0, // RPC的请求和响应
        RSP_RPC,
        REQ_TOPIC, // 主题的请求和响应
        RSP_TOPIC,
        REQ_SERVICE, // 服务的请求和响应
        RSP_SERVICE
    };
    enum class RCode
    {
        OK = 0,
        PARSE_FAILED, // 解析失败
        ERROR_MSGTYPE,
        INVALID_MSG,       // 消息无效
        DISCONNECTED,      // 连接断开
        INVALID_PARAMS,    // 无效参数
        NOT_FOUND_SERVICE, // 服务不存在
        INVALID_OPTYPE,    // 无效主题类型
        NOT_FOUND_TOPIC,   // 主题不存在
        INTERNAL_ERROR
    };
    static std::string ErrReason(RCode code)
    {
        std::unordered_map<RCode, std::string> err_map =
            {
                {RCode::OK, "成功处理！"},
                {RCode::PARSE_FAILED, "消息解析失败！"},
                {RCode::ERROR_MSGTYPE, "消息类型错误！"},
                {RCode::INVALID_MSG, "无效消息"},
                {RCode::DISCONNECTED, "连接已断开！"},
                {RCode::INVALID_PARAMS, "无效的Rpc参数!"},
                {RCode::NOT_FOUND_SERVICE, "没有找到对应的服务！"},
                {RCode::INVALID_OPTYPE, "无效的操作类型"},
                {RCode::NOT_FOUND_TOPIC, "没有找到对应的主题！"},
                {RCode::INTERNAL_ERROR, "内部错误！"}};
        auto it = err_map.find(code);
        if (it == err_map.end())
        {
            return "未知错误";
        }
        return it->second;
    }

    // rpc消息的操作类型
    enum class RpcType
    {
        REQ_SYNC = 0, // 同步请求
        REQ_ASYNC,    // 异步请求
        REQ_CALLBACK  // 回调请求
    };

    // topic消息的操作类型
    enum class TopicOptype
    {
        TOPIC_CREATE = 0,
        TOCPIC_REMOVE, // 删除主题
        TOPIC_SUBSCRIBE,
        TOPIC_CANCEL, // 取消订阅
        TOPIC_PUBLISH // 消息发布
    };

    // Service消息的操作类型
    enum class ServiceOptype
    {
        SERVICE_REGISTRY = 0,
        SERVICE_ONLINE,
        SERVICE_OFFLINE,
        SERVICE_DISCOVERY,
        SERVICE_UNKOWNN

    };

}