#ifndef CLOUDLM_CLOUD_FORMAT__
#define CLOUDLM_CLOUD_FORMAT__

#include "cloud-lm/model_type.hh"
#include "cloud-lm/weights.hh"

#include <string>
#include <vector>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

namespace cloudlm {
namespace ngram {

struct Request {
	std::string url; // For instance http://example.org:8080/solr
	std::string base_url; // For instance example.org
	std::string port; // For instance 8080. If empty -> http
};

struct Data {
	std::string gram;
	int order;
};

class ClientSocket {
public:
	ClientSocket();
	void OpenConnection(std::string ip, std::string port);
	void HandlerConnect(const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
	void HandleWriteRequest(const boost::system::error_code& err);
	void HandleReadStatusLine(const boost::system::error_code& err);
	void HandleReadHeaders(const boost::system::error_code& err);
	void HandleReadContent(const boost::system::error_code& err);
	void SendRequest(std::string message, std::string url, std::stringstream &solr_response);

	/// SOCKET ///
	boost::asio::io_service io_service_;
	tcp::socket socket_;
	tcp::resolver::iterator iterator_;
	boost::asio::streambuf request_;
	boost::asio::streambuf response_;
	bool is_setup;
};

void doSplitPhrasesTimer(int action);

void UpdateRequestStats(const int order);

void UpdateRequestStats(const std::string gram, const int order);

void ShowStats();

std::string EncodeSolrSpecialCharacters(const std::string & sSrc);

std::string UriEncode(const std::string & sSrc);

void SendRequestSolr(std::string search, std::stringstream &response);

bool SendRequest(Data &req, ProbBackoff &gram);

void SendRequest(std::vector<std::string> words);

bool ReadJson(std::stringstream &json, ProbBackoff &gram);

bool ReadJson(std::stringstream &json);

void AddToCache(const std::string key, const ProbBackoff value);

bool ExistInCache(const std::string key);

bool GetFromCache(const std::string key, ProbBackoff &gram);

} // namespace ngram
} // namespace cloudlm
#endif // CLOUDLM_CLOUD_FORMAT__
