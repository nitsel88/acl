#pragma once
#include "acl_cpp/acl_cpp_define.hpp"
#include "acl_cpp/connpool/connect_pool.hpp"

namespace acl
{

/**
 * memcache 客户端连接池类，该类父类为 connect_pool，该类只需实现父类中的虚函数
 * create_connect 便拥有了连接池父类 connect_pool 的功能；另外，该类创建
 * 的连接对象是 memcache 对象，所以在调用 connect_pool::peek 时返回
 * 的便是 http_request 类，调用者需要将 peek 返回的类对象强制转为 memcache
 * 类对象，便可以使用 memcache 类折所有功能，其中 memcache 类为
 * connect_client 的子类
 */
class ACL_CPP_API memcache_pool : public connect_pool
{
public:
	/**
	 * 构造函数
	 * @param addr {const char*} 服务端地址，格式：ip:port
	 * @param count {size_t} 连接池的最大连接数限制，当该值为 0 时则没有限制
	 * @param idx {size_t} 该连接池对象在集合中的下标位置(从 0 开始)
	 */
	memcache_pool(const char* addr, size_t count, size_t idx = 0);
	~memcache_pool();

	/**
	 * 设置网络连接超时时间及网络 IO 读写超时时间(秒)
	 * @param conn_timeout {int} 连接超时时间
	 * @param rw_timeout {int} 网络 IO 读写超时时间(秒)
	 * @return {memcache_pool&}
	 */
	memcache_pool& set_timeout(int conn_timeout = 30, int rw_timeout = 60);

protected:
	// 基类纯虚函数
	virtual connect_client* create_connect();

private:
	int   conn_timeout_;
	int   rw_timeout_;
};

} // namespace acl
