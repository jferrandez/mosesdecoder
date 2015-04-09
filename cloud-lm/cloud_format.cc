#include "cloud-lm/cloud_format.hh"
#include "cloud-lm/config.hh"

#include "cloud-lm/blank.hh"
#include "cloud-lm/lm_exception.hh"

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/algorithm/string/replace.hpp>

using boost::asio::ip::tcp;

namespace cloudlm {
namespace ngram {

// To show a histogram with the frequency of requests to Solr
typedef boost::unordered_map< std::string, int > HistogramWord;
typedef boost::unordered_map< int, int > HistogramOrder;
HistogramWord hist_word;
HistogramOrder hist_order;

////////// CACHE /////////////
/// Typedef for URL/Entry pair
typedef std::pair< std::string, ProbBackoff > EntryPair;

/// Typedef for Cache list
typedef std::list< EntryPair > CacheList;

/// Typedef for URL-indexed map into the CacheList
typedef boost::unordered_map< std::string, CacheList::iterator > CacheMap;

/// Cache LRU list
CacheList mCacheList;

/// Cache map into the list
CacheMap mCacheMap;

/// Max entries in Cache
uint64_t mMaxEntries = 100000000;
/// Current entries
uint64_t mEntries = 0;
///////// END CACHE //////////

bool ValidateUrl(const char *url, Request &request) {
	// TODO use regex instead strncmp and get port and baseurl
	//std::string str_url = url;
	//static const boost::regex re("^(.*:)//([a-z.-]+)(:[0-9]+)/solr$"); //toy regular expression, enough for our purposes currently
	//return boost::regex_match(str_url, re);
	return strncmp(url, "http://", 7) == 0;
}

void UpdateRequestStats(const std::string gram, const int order) {
    hist_word[gram] = hist_word[gram] + 1;
    hist_order[order] = hist_order[order] + 1;
}

void ShowStats() {
	std::cerr << "START STATS" << std::endl;
	std::cerr << "--NGRAMS--" << std::endl;


	BOOST_FOREACH(HistogramWord::value_type i, hist_word) {
	    std::cerr << i.first << "\t" << i.second << std::endl;
	}

	std::cerr << std::endl << std::endl << "--ORDER--" << std::endl;
	BOOST_FOREACH(HistogramOrder::value_type i, hist_order) {
	    std::cerr << i.first << "\t" << i.second << std::endl;
	}
	std::cerr << "FINISH STATS" << std::endl;
}

std::string UriEncode(const std::string & sSrc)
{
	// + - && || ! ( ) { } [ ] ^ " ~ * ? : \ SPECIAL CHARACTERS
	// TODO do that with boost regular expression
	std::string sResult = boost::replace_all_copy(sSrc, "\\", "\\\\");
	boost::replace_all(sResult, "+", "\\+");
	boost::replace_all(sResult, "-", "\\-");
	boost::replace_all(sResult, "&", "\\&");
	boost::replace_all(sResult, "#", "\\#");
	boost::replace_all(sResult, "|", "\\|");
	boost::replace_all(sResult, "!", "\\!");
	boost::replace_all(sResult, "(", "\\(");
	boost::replace_all(sResult, ")", "\\)");
	boost::replace_all(sResult, "{", "\\{");
	boost::replace_all(sResult, "}", "\\}");
	boost::replace_all(sResult, "[", "\\[");
	boost::replace_all(sResult, "]", "\\]");
	boost::replace_all(sResult, "^", "\\^");
	boost::replace_all(sResult, "\"", "\\\"");
	boost::replace_all(sResult, "~", "\\~");
	boost::replace_all(sResult, "*", "\\*");
	boost::replace_all(sResult, "?", "\\?");
	boost::replace_all(sResult, ":", "\\:");

	std::stringstream escaped;
	escaped.fill('0');
	escaped << std::hex;

	for (std::string::const_iterator i = sResult.begin(), n = sResult.end(); i != n; ++i) {
		std::string::value_type c = (*i);

		// Keep alphanumeric and other accepted characters intact
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
			continue;
		}

		// Any other characters are percent-encoded
		escaped << '%' << std::setw(2) << int((unsigned char) c);
	}

	return escaped.str();
}

bool SendRequest(Data &req, ProbBackoff &gram) {
  if (GetFromCache(req.gram, gram)) return true;
  //try
  {
    cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
	//UpdateRequestStats(req.gram, req.order);

	boost::asio::io_service io_service;

	// Get a list of endpoints corresponding to the server name.
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(config->request.base_url, config->request.port);
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;

	// Try each endpoint until we successfully establish a connection.
	tcp::socket socket(io_service);
	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && endpoint_iterator != end)
	{
	  socket.close();
	  socket.connect(*endpoint_iterator++, error);
	}
	if (error)
	  throw boost::system::system_error(error);

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << "GET " << "/solr/collection1/select?q=gram:\"" << UriEncode(req.gram) << "\"&fq=order:" << req.order << "&wt=json" << " HTTP/1.0\r\n";
	request_stream << "Host: " << config->request.url << "\r\n";
	request_stream << "Accept: */*;charset=UTF-8\r\nAccept-Charset: UTF-8\r\nConnection: close\r\n\r\n";

	//std::cerr << "REQUEST: " << UriEncode(req.gram) << std::endl;

	// Send the request.
	boost::asio::write(socket, request);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	boost::asio::streambuf response;
	//boost::asio::read_until(socket, response, "\r\n");
	// Read until EOF, writing data to output as we go.
	while (boost::asio::read(socket, response,
		  boost::asio::transfer_at_least(1), error)) {}
	  //std::cout << &response;
	if (error != boost::asio::error::eof)
	  throw boost::system::system_error(error);

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		// TODO: change the failed requests
	  //throw boost::system::system_error(boost::asio::error::no_data);
	  std::cerr << "Invalid response\n" << UriEncode(req.gram) << std::endl;
	  return false;
	}
	if (status_code != 200)
	{
		// TODO: change the failed requests
	  //throw boost::system::system_error(boost::asio::error::connection_aborted);
	  std::cerr << "Response returned with status code " << status_code << "\n" << UriEncode(req.gram) << std::endl;
	  return false;
	}

	// Read the response headers, which are terminated by a blank line.
	boost::asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	while (std::getline(response_stream, header) && header != "\r") {}
	  //std::cout << header << "\n";
	//std::cout << "\n";

	std::stringstream body_stream;
	std::string body_response;
	while (std::getline(response_stream, body_response)) {
		body_stream << body_response;
	}

	if (!ReadJson(body_stream, gram)) return false;
	// TODO: Quitar cuando se arregle en Solr
	if (gram.backoff == ngram::kExtensionBackoff) gram.backoff = ngram::kNoExtensionBackoff;
	AddToCache(req.gram, gram);
	/* TODO: we put the not found words / phrases in the cache --- WE CAN'T PUT for the moment -> GetCache returns true when "not found" word / phrase is found in the cache.
	bool gram_found = ReadJson(body_stream, gram);
	// TODO: Quitar cuando se arregle en Solr
	if (gram.backoff == ngram::kExtensionBackoff) gram.backoff = ngram::kNoExtensionBackoff;
	AddToCache(req.gram, gram);
	if (!gram_found) return false;
	*/

	// Write whatever content we already have to output.
	//if (response.size() > 0)
	//  std::cout << &response;
  }
  /*catch (std::exception& e)
  {
	UTIL_THROW(SolrException, "Solr request - something is wrong:\n" << e.what());
	return false;
  }*/
  return true;
}

bool SendRequest(Data &req, std::vector<std::string> words) {
	//ProbBackoff ignored_prob;
	//if (GetFromCache(req.gram, ignored_prob)) return true;

    cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
	int expected_results = 0;
	std::stringstream search;
	for (int n = 1; n <= req.order; n++) {
		std::string search_per_order = "";
		signed int limit = words.size() - n + 1;
		for (int i = 0; i < limit; i++) {
			std::string search_per_gram = "";
			for (int j = i; j < i + n; j++) {
				search_per_gram += (j > i ? " " : "") + UriEncode(words[j]);
			}
			if (search_per_gram != ""){
				search_per_order += "gram:\"" + search_per_gram + "\" ";
				expected_results++;
			}
		}
		if (search_per_order != "") {
			search << "((" << search_per_order << ") AND order:" << n << ") ";
		}
	}
	std::string s_search = search.str();
	//std::cerr << "SEARCH TO DO: " << s_search << std::endl;
	if (s_search == "") return false;
	boost::replace_all(s_search, " ", "%20");

	boost::asio::io_service io_service;

	// Get a list of endpoints corresponding to the server name.
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(config->request.base_url, config->request.port);
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;

	// Try each endpoint until we successfully establish a connection.
	tcp::socket socket(io_service);
	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && endpoint_iterator != end)
	{
	  socket.close();
	  socket.connect(*endpoint_iterator++, error);
	}
	if (error)
	  throw boost::system::system_error(error);

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << "GET " << "/solr/collection1/select?q=" << s_search << "&rows:" << expected_results << "&wt=json" << " HTTP/1.0\r\n";
	request_stream << "Host: " << config->request.url << "\r\n";
	request_stream << "Accept: */*;charset=UTF-8\r\nAccept-Charset: UTF-8\r\nConnection: close\r\n\r\n";

	//std::cerr << "REQUEST: " << UriEncode(req.gram) << std::endl;

	// Send the request.
	boost::asio::write(socket, request);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	boost::asio::streambuf response;
	//boost::asio::read_until(socket, response, "\r\n");
	// Read until EOF, writing data to output as we go.
	while (boost::asio::read(socket, response,
		  boost::asio::transfer_at_least(1), error)) {}
	  //std::cout << &response;
	if (error != boost::asio::error::eof)
	  throw boost::system::system_error(error);

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		// TODO: change the failed requests
	  //throw boost::system::system_error(boost::asio::error::no_data);
	  std::cerr << "Invalid response\n" << UriEncode(req.gram) << std::endl;
	  return false;
	}
	if (status_code != 200)
	{
		// TODO: change the failed requests
	  //throw boost::system::system_error(boost::asio::error::connection_aborted);
	  std::cerr << "Response returned with status code " << status_code << "\n" << UriEncode(req.gram) << std::endl;
	  return false;
	}

	// Read the response headers, which are terminated by a blank line.
	boost::asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	while (std::getline(response_stream, header) && header != "\r") {}
	  //std::cout << header << "\n";
	//std::cout << "\n";

	std::stringstream body_stream;
	std::string body_response;
	while (std::getline(response_stream, body_response)) {
		body_stream << body_response;
	}

	return ReadJson(body_stream);
}

bool ReadJson(std::stringstream &json, ProbBackoff &gram) {
	//try
	{
		boost::property_tree::ptree pt;
		boost::property_tree::read_json(json, pt);

		int numFound = pt.get<int>("response.numFound", 0);

		if (numFound == 0) return false;
		BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("response.docs."))
		{
			assert(v.first.empty()); // array elements have no names
			gram.prob = v.second.get<float>("prob");
			gram.backoff = v.second.get<float>("backoff", ngram::kNoExtensionBackoff);

			/* PRINT for debug
			 void print(boost::property_tree::ptree const& pt)
			{
				using boost::property_tree::ptree;
				ptree::const_iterator end = pt.end();
				for (ptree::const_iterator it = pt.begin(); it != end; ++it) {
					std::cout << it->first << ": " << it->second.get_value<std::string>() << std::endl;
					print(it->second);
				}
			}
			 */
		}
	}
	/*catch (std::exception& e)
	{
		UTIL_THROW(SolrException, "Solr request - something is wrong:\n" << e.what());
		return false;
	}*/
	return true;
}

bool ReadJson(std::stringstream &json) {
	{
		boost::property_tree::ptree pt;
		boost::property_tree::read_json(json, pt);

		int numFound = pt.get<int>("response.numFound", 0);

		if (numFound == 0) return false;
		BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("response.docs."))
		{
			assert(v.first.empty()); // array elements have no names
			ProbBackoff gram;
			std::string s_gram;
			s_gram = v.second.get<std::string>("gram");
			gram.prob = v.second.get<float>("prob");
			gram.backoff = v.second.get<float>("backoff", ngram::kNoExtensionBackoff);
			AddToCache(s_gram, gram);

			/* PRINT for debug
			 void print(boost::property_tree::ptree const& pt)
			{
				using boost::property_tree::ptree;
				ptree::const_iterator end = pt.end();
				for (ptree::const_iterator it = pt.begin(); it != end; ++it) {
					std::cout << it->first << ": " << it->second.get_value<std::string>() << std::endl;
					print(it->second);
				}
			}
			 */
		}
	}
	return true;
}

void AddToCache(std::string key, const ProbBackoff value) {
	// push it to the front;
	mCacheList.push_front( std::make_pair( key, value ) );

	// add it to the cache map
	mCacheMap[ key ] = mCacheList.begin();

	// increase count of entries
	mEntries++;

	// check if it's time to remove the last element
	if ( mEntries > mMaxEntries )
	{
	    // erase from the map the last cache list element
	    mCacheMap.erase( mCacheList.back().first );

	    // erase it from the list
	    mCacheList.pop_back();

	    // decrease count
	    mEntries--;
	}
}

bool GetFromCache(const std::string key, ProbBackoff &gram) {
	CacheMap::const_iterator found = mCacheMap.find(key);
	if (found == mCacheMap.end()) {
		return false;
	}
	else {
		ProbBackoff not_found_gram;
		if (not_found_gram.prob == found->second->second.prob && not_found_gram.backoff == found->second->second.backoff) {
			return false;
		}
		//std::cout << "GOOD JOB: " << key << " - " << found->second->second.prob;
		//std::cout << " - Entries: " << mEntries << std::endl;
		gram.prob = found->second->second.prob;
		gram.backoff = found->second->second.backoff;
	}
	return true;
}

} // namespace ngram
} // namespace cloudlm
