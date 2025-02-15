// main.cpp : 定义控制台应用程序的入口点。
//
#include "stdafx.h"
#include <assert.h>
#include "lib_acl.h"
#include "acl_cpp/lib_acl.hpp"

using namespace acl;

//////////////////////////////////////////////////////////////////////////

class http_servlet : public HttpServlet
{
public:
	http_servlet(socket_stream* stream, session& session)
		: HttpServlet(stream, &session)
	{
		param1_ = NULL;
		param2_ = NULL;
		param3_ = NULL;
		file1_ = NULL;
		file2_ = NULL;
		file3_ = NULL;
	}

	~http_servlet(void)
	{
	}

	// GET 方法
	virtual bool doGet(HttpServletRequest& req, HttpServletResponse& res)
	{
		return doPost(req, res);
	}

	// POST 方法
	virtual bool doPost(HttpServletRequest& req, HttpServletResponse& res)
	{
		// 如果 session 项不存在，则设置
#if 0
		const char* sid = req.getSession().getAttribute("sid");
		if (*sid == 0)
			req.getSession().setAttribute("sid", "xxxxxx");
#endif

		// 创建 HTTP 响应头
                res.addCookie("name1", "value1");
		res.addCookie("name2", "value2", ".test.com", "/", 3600 * 24);
//		res.setStatus(400);  // 可以设置返回的状态码

		// 两种方式都可以设置字符集
		if (0)
			res.setContentType("text/xml; charset=gb2312");
		else
		{
			res.setContentType("text/xml");
			res.setCharacterEncoding("gb2312");
		}

		// 获得 HTTP 请求的数据类型，正常的参数类型，即 name&value 方式
		// 还是 MIME 数据类型，还是数据流类型
		http_request_t request_type = req.getRequestType();
		if (request_type == HTTP_REQUEST_NORMAL)
			return doParams(req, res);
		else if (request_type == HTTP_REQUEST_MULTIPART_FORM)
			return doUpload(req, res);
		assert(request_type == HTTP_REQUEST_OCTET_STREAM);
		return doOctetStream(req, res);
	}

	bool doResponse(HttpServletRequest& req, HttpServletResponse& res)
	{
		// 获得浏览器传来的 cookie 值
		const char* cookie1 = req.getCookieValue("name1");
		const char* cookie2 = req.getCookieValue("name2");

		// 获得 sid session 值
#if 0
		const char* sid = req.getSession().getAttribute("sid");
#else
		const char* sid = "test_sid";
#endif

		// 创建 xml 格式的数据体
		xml body;
		body.get_root().add_child("root", true)
			.add_child("content_type", true)
				.add_attr("type", (int) req.getRequestType())
				.get_parent()
			.add_child("sessions", true)
				.add_child("session", true)
					.add_attr("sid", sid ? sid : "null")
					.get_parent()
				.get_parent()
			.add_child("cookies", true)
				.add_child("cookie", true)
					.add_attr("name1", cookie1 ? cookie1 : "null")
					.get_parent()
				.add_child("cookie", true)
					.add_attr("name2", cookie2 ? cookie2 : "null")
					.get_parent()
				.get_parent()
			.add_child("params", true)
				.add_child("param", true)
					.add_attr("name1", param1_ ? param1_ : "null")
					.get_parent()
				.add_child("param", true)
					.add_attr("name2", param2_ ? param2_ : "null")
					.get_parent()
				.add_child("param", true)
					.add_attr("name3", param3_ ? param3_ : "null")
					.get_parent()
				.get_parent()
			.add_child("files", true)
				.add_child("file", true)
					.add_attr("filename", file1_ ? file1_ : "null")
					.get_parent()
				.add_child("file", true)
					.add_attr("filename", file2_ ? file2_ : "null")
					.get_parent()
				.add_child("file", true)
					.add_attr("filename", file3_ ? file3_ : "null");
		string buf;
		body.build_xml(buf);

		// 发送 http 响应头
		if (res.sendHeader() == false)
			return false;
		// 发送 http 响应体
		if (res.getOutputStream().write(buf) == -1)
			return false;
		return true;
	}

	// GET 方式或 POST 方式且满足：
	// Content-Type: application/x-www-form-urlencoded
	bool doParams(HttpServletRequest& req, HttpServletResponse& res)
	{
		param1_ = req.getParameter("name1");
		param2_ = req.getParameter("name2");
		param3_ = req.getParameter("name2");
		return doResponse(req, res);
	}

	// POST 方式且满足：
	// Content-Type: multipart/form-data; boundary=xxx
	bool doUpload(HttpServletRequest& req, HttpServletResponse& res)
	{
		// 先获得 Content-Type 对应的 http_ctype 对象
		http_mime* mime = req.getHttpMime();
		if (mime == NULL)
		{
			logger_error("http_mime null");
			return false;
		}

		// 获得数据体的长度
		long long int len = req.getContentLength();
		if (len <= 0)
		{
			logger_error("body empty");
			return false;
		}

		// 获得输入流
		istream& in = req.getInputStream();
		char  buf[8192];
		int   ret;
		bool  finish = false;

		const char* filepath = "./var/mime_file";
		ofstream out;
		out.open_write(filepath);

		// 设置原始文件存入路径
		mime->set_saved_path(filepath);

		size_t k;

		// 读取 HTTP 客户端请求数据
		while (len > 0)
		{
			k = (size_t) len > sizeof(buf)
				? sizeof(buf) : (size_t) len;
			ret = in.read(buf, k, false);
			if (ret == -1)
			{
				logger_error("read POST data error");
				return false;
			}
			out.write(buf, ret);

			len -= ret;

			// 将读得到的数据输入至解析器进行解析
			if (!finish && mime->update(buf, ret) == true)
				finish = true;
		}
		out.close();

		if (len != 0 || finish == false)
			logger_warn("not read all data from client");

		param1_ = req.getParameter("name1");
		param2_ = req.getParameter("name2");
		param3_ = req.getParameter("name3");

		string path;

		// 遍历所有的 MIME 结点，找出其中为文件结点的部分进行转储
		const std::list<http_mime_node*>& nodes = mime->get_nodes();
		std::list<http_mime_node*>::const_iterator cit = nodes.begin();
		for (; cit != nodes.end(); ++cit)
		{
			const char* name = (*cit)->get_name();
			if (name == NULL)
				continue;

			http_mime_t mime_type = (*cit)->get_mime_type();
			if (mime_type == HTTP_MIME_FILE)
			{
				const char* filename = (*cit)->get_filename();
				if (filename == NULL)
				{
					logger("filename null");
					continue;
				}

				if (strcmp(name, "file1") == 0)
					file1_ = filename;
				else if (strcmp(name, "file2") == 0)
					file2_ = filename;
				else if (strcmp(name, "file3") == 0)
					file3_ = filename;

				// 有的浏览器（如IE）上传文件时会带着文件路径，所以
				// 需要先将路径去掉
				filename = acl_safe_basename(filename);
#ifdef WIN32
				path.format("var\\%s", filename);
#else
				path.format("./var/%s", filename);
#endif
				(void) (*cit)->save(path.c_str());
			}
		}

		// 查找上载的某个文件并转储
		const http_mime_node* node = mime->get_node("file1");
		if (node && node->get_mime_type() == HTTP_MIME_FILE)
		{
			const char* ptr = node->get_filename();
			if (ptr)
			{
				// 有的浏览器（如IE）上传文件时会带着文件路径，所以
				// 需要先将路径去掉
				ptr = acl_safe_basename(ptr);
#ifdef WIN32
				path.format(".\\var\\1_%s", ptr);
#else
				path.format("./var/1_%s", ptr);
#endif
				(void) node->save(path.c_str());
			}
		}
		return doResponse(req, res);
	}

	// POST 方式且满足：
	// Content-Type: application/octet-stream
	bool doOctetStream(HttpServletRequest&, HttpServletResponse&)
	{
		return false;
	}

protected:
private:
	const char* param1_;
	const char* param2_;
	const char* param3_;
	const char* file1_;
	const char* file2_;
	const char* file3_;
};

//////////////////////////////////////////////////////////////////////////

static void do_run(socket_stream* stream)
{
	memcache_session session("127.0.0.1:11211");
	http_servlet servlet(stream, session);
	servlet.setLocalCharset("gb2312");
	servlet.doRun();
}

// 服务器方式运行时的服务类
class master_service : public master_proc
{
public:
	master_service() {}
	~master_service() {}
protected:
	virtual void on_accept(socket_stream* stream)
	{
		do_run(stream);
	}
};

// WEB 服务模式
static void do_alone(void)
{
	master_service service;
	acl::log::stdout_open(true);
	const char* addr = "0.0.0.0:8081";
	printf("listen: %s ...\r\n", addr);
	service.run_alone(addr, NULL, 0);  // 单独运行方式
}

// WEB CGI 模式
static void do_cgi(void)
{
	do_run(NULL);
}

//////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
#ifdef WIN32
	acl::acl_cpp_init();
#endif

	// 开始运行
	if (argc >= 2 && strcmp(argv[1], "alone") == 0)
		do_alone();
	else
		do_cgi();

	return 0;
}

