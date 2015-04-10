#ifndef CLOUDLM_CLOUD_FORMAT__
#define CLOUDLM_CLOUD_FORMAT__

#include "cloud-lm/model_type.hh"
#include "cloud-lm/weights.hh"

#include <string>
#include <vector>

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

bool ValidateUrl(const char *url, Request &request);

void UpdateRequestStats(const std::string gram, const int order);

void ShowStats();

std::string EncodeSolrSpecialCharacters(const std::string & sSrc);

std::string UriEncode(const std::string & sSrc);

void SendRequestSolr(std::string search, std::stringstream &response);

bool SendRequest(Data &req, ProbBackoff &gram);

void SendRequest(Data &req, std::vector<std::string> words);

bool ReadJson(std::stringstream &json, ProbBackoff &gram);

bool ReadJson(std::stringstream &json);

void AddToCache(const std::string key, const ProbBackoff value);

bool ExistInCache(const std::string key);

bool GetFromCache(const std::string key, ProbBackoff &gram);

} // namespace ngram
} // namespace cloudlm
#endif // CLOUDLM_CLOUD_FORMAT__
