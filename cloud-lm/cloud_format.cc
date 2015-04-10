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
//#include <boost/regex.hpp>

using boost::asio::ip::tcp;
using namespace std;

namespace cloudlm {
namespace ngram {

// Regex with special characters from Solr
//boost::regex solr_special_characters("([\\+-&#|!\(\)\{\}\[\]\^\"~*?:])");

// To show a histogram with the frequency of requests to Solr
typedef boost::unordered_map< string, int > HistogramWord;
typedef boost::unordered_map< int, int > HistogramOrder;
HistogramWord hist_word;
HistogramOrder hist_order;

////////// CACHE /////////////
/// Typedef for URL/Entry pair
typedef pair< string, ProbBackoff > EntryPair;

/// Typedef for Cache list
typedef list< EntryPair > CacheList;

/// Typedef for URL-indexed map into the CacheList
typedef boost::unordered_map< string, CacheList::iterator > CacheMap;

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
	//string str_url = url;
	//static const boost::regex re("^(.*:)//([a-z.-]+)(:[0-9]+)/solr$"); //toy regular expression, enough for our purposes currently
	//return boost::regex_match(str_url, re);
	return strncmp(url, "http://", 7) == 0;
}

void UpdateRequestStats(const string gram, const int order) {
    hist_word[gram] = hist_word[gram] + 1;
    hist_order[order] = hist_order[order] + 1;
}

void ShowStats() {
	cerr << "START STATS" << endl;
	cerr << "--NGRAMS--" << endl;


	BOOST_FOREACH(HistogramWord::value_type i, hist_word) {
	    cerr << i.first << "\t" << i.second << endl;
	}

	cerr << endl << endl << "--ORDER--" << endl;
	BOOST_FOREACH(HistogramOrder::value_type i, hist_order) {
	    cerr << i.first << "\t" << i.second << endl;
	}
	cerr << "FINISH STATS" << endl;
}

string EncodeSolrSpecialCharacters(const string & sSrc) {
	// + - && || ! ( ) { } [ ] ^ " ~ * ? : \ SPECIAL CHARACTERS
	// TODO do that with boost regular expression (something is wrong in Moses when compiling with boost_regex
	// We need to escape Solr special characters
	//string sResult = boost::regex_replace(sSrc, solr_special_characters, "\g1");
	string sResult = boost::replace_all_copy(sSrc, "\\", "\\\\");
	boost::replace_all(sResult, "\"", "\\\"");
	/*boost::replace_all(sResult, "+", "\\+");
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
	boost::replace_all(sResult, "~", "\\~");
	boost::replace_all(sResult, "*", "\\*");
	boost::replace_all(sResult, "?", "\\?");
	boost::replace_all(sResult, ":", "\\:");*/

	return sResult;
}

string UriEncode(const string & sSrc)
{

	stringstream escaped;
	escaped.fill('0');
	escaped << hex;

	// We need to encode the search in the URL
	for (string::const_iterator i = sSrc.begin(), n = sSrc.end(); i != n; ++i) {
		string::value_type c = (*i);

		// Keep alphanumeric and other accepted characters intact
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
			continue;
		}

		// Any other characters are percent-encoded
		escaped << '%' << setw(2) << int((unsigned char) c);
	}

	return escaped.str();
}

 void SendRequestSolr(string search, stringstream &solr_response) {
  try
  {
    cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();

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
	ostream request_stream(&request);
	//request_stream << "GET " << "/solr/collection1/select?q=gram:\"" << UriEncode(req.gram) << "\"&fq=order:" << req.order << "&wt=json" << " HTTP/1.0\r\n";
	request_stream << "GET " << "/solr/collection1/select?wt=json&" << search << " HTTP/1.0\r\n";
	request_stream << "Host: " << config->request.url << "\r\n";
	request_stream << "Accept: */*;charset=UTF-8\r\nAccept-Charset: UTF-8\r\nConnection: close\r\n\r\n";

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
	  //cout << &response;
	if (error != boost::asio::error::eof)
	  throw boost::system::system_error(error);

	// Check that response is OK.
	istream response_stream(&response);
	string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	string status_message;
	getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		// TODO: change the failed requests
	  //throw boost::system::system_error(boost::asio::error::no_data);
	  cerr << "Invalid response\n" << search << endl;
	  return;
	}
	if (status_code != 200)
	{
		// TODO: change the failed requests
	  //throw boost::system::system_error(boost::asio::error::connection_aborted);
	  cerr << "Response returned with status code " << status_code << "\n" << search << endl;
	  return;
	}

	// Read the response headers, which are terminated by a blank line.
	boost::asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	string header;
	while (getline(response_stream, header) && header != "\r") {}

	string body_response;
	while (getline(response_stream, body_response)) {
		solr_response << body_response;
	}
  }
  catch (exception& e)
  {
	UTIL_THROW(SolrException, "Solr request - something is wrong:\n" << e.what());
	return;
  }
}

bool SendRequest(Data &req, ProbBackoff &gram) {
	if (GetFromCache(req.gram, gram)) return true;

	stringstream query;
	query << "q=gram:\"" << UriEncode(EncodeSolrSpecialCharacters(req.gram)) << "\"&fq=order:" << req.order;

	/*if (!ReadJson(body_stream, gram)) return false;
	// TODO: Quitar cuando se arregle en Solr
	if (gram.backoff == ngram::kExtensionBackoff) gram.backoff = ngram::kNoExtensionBackoff;
	AddToCache(req.gram, gram);*/
	// TODO: we put the not found words / phrases in the cache --- WE CAN'T PUT for the moment -> GetCache returns true when "not found" word / phrase is found in the cache.
	stringstream response;
	SendRequestSolr(query.str(), response);
	bool gram_found = ReadJson(response, gram);
	// TODO: Quitar cuando se arregle en Solr
	if (!gram_found && gram.backoff == ngram::kExtensionBackoff) gram.backoff = ngram::kNoExtensionBackoff;
	AddToCache(req.gram, gram);
	if (!gram_found) return false;

	return true;
}

void SendRequest(Data &req, vector<string> words) {
	int expected_results = 0;
	for (int n = 1; n <= req.order; n++) { // for each n-gram order
		string search_per_order = "";
		signed int limit = words.size() - n + 1;
		// get all the possible n-grams for this order
		for (int i = 0; i < limit; i++) {
			string search_per_gram = "";
			for (int j = i; j < i + n; j++) {
				search_per_gram += (j > i ? " " : "") + words[j];
			}
			if (search_per_gram != "" && !ExistInCache(search_per_order)){
				search_per_order += "gram:\"" + search_per_gram + "\" ";
				expected_results++;
			}
		}
		if (search_per_order != "") {
			stringstream search;
			search << "q=" << UriEncode(search_per_order) << "&fq=order:" << n << "&rows:" << expected_results;
			stringstream response;
			SendRequestSolr(search.str(), response);
			ReadJson(response);
		}
	}


	//request_stream << "GET " << "/solr/collection1/select?q=" << s_search << "&rows:" << expected_results << "&wt=json" << " HTTP/1.0\r\n";

}

// TODO: quitar esta? Es para hacer todas las queries de una vez.
bool SendRequest2(Data &req, vector<string> words) {
	int expected_results = 0;
	stringstream search;
	for (int n = 1; n <= req.order; n++) { // for each n-gram order
		string search_per_order = "";
		signed int limit = words.size() - n + 1;
		// get all the possible n-grams for this order
		for (int i = 0; i < limit; i++) {
			string search_per_gram = "";
			for (int j = i; j < i + n; j++) {
				search_per_gram += (j > i ? " " : "") + words[j];
			}
			if (search_per_gram != "" && !ExistInCache(search_per_order)){
				search_per_order += "gram:\"" + search_per_gram + "\" ";
				expected_results++;
			}
		}
		if (search_per_order != "") {
			search << "q=" << search_per_order << "&fq=order:" << n << "";
		}
	}
	string s_search = search.str();
	//cerr << "SEARCH TO DO: " << s_search << endl;
	if (s_search == "") return false;
	boost::replace_all(s_search, " ", "%20");
	return true;

	//request_stream << "GET " << "/solr/collection1/select?q=" << s_search << "&rows:" << expected_results << "&wt=json" << " HTTP/1.0\r\n";


	//return ReadJson(body_stream);
}

bool ReadJson(stringstream &json, ProbBackoff &gram) {
	//try
	{
		boost::property_tree::ptree pt;
		boost::property_tree::read_json(json, pt);

		int numFound = pt.get<int>("response.numFound", 0);

		if (numFound == 0){
			gram.prob = 0;
			gram.backoff = ngram::kNoExtensionBackoff;
			return false;
		}
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
					cout << it->first << ": " << it->second.get_value<string>() << endl;
					print(it->second);
				}
			}
			 */
		}
	}
	/*catch (exception& e)
	{
		UTIL_THROW(SolrException, "Solr request - something is wrong:\n" << e.what());
		return false;
	}*/
	return true;
}

bool ReadJson(stringstream &json) {
	{
		boost::property_tree::ptree pt;
		boost::property_tree::read_json(json, pt);

		int numFound = pt.get<int>("response.numFound", 0);

		if (numFound == 0) return false;
		BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("response.docs."))
		{
			assert(v.first.empty()); // array elements have no names
			ProbBackoff gram;
			string s_gram;
			s_gram = v.second.get<string>("gram");
			gram.prob = v.second.get<float>("prob");
			gram.backoff = v.second.get<float>("backoff", ngram::kNoExtensionBackoff);
			AddToCache(s_gram, gram);

			/* PRINT for debug
			 void print(boost::property_tree::ptree const& pt)
			{
				using boost::property_tree::ptree;
				ptree::const_iterator end = pt.end();
				for (ptree::const_iterator it = pt.begin(); it != end; ++it) {
					cout << it->first << ": " << it->second.get_value<string>() << endl;
					print(it->second);
				}
			}
			 */
		}
	}
	return true;
}

void AddToCache(string key, const ProbBackoff value) {
	// push it to the front;
	mCacheList.push_front( make_pair( key, value ) );

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



bool ExistInCache(const string key) {
	CacheMap::const_iterator found = mCacheMap.find(key);
	if (found == mCacheMap.end()) return false;
	else return true;
}

bool GetFromCache(const string key, ProbBackoff &gram) {
	CacheMap::const_iterator found = mCacheMap.find(key);
	if (found == mCacheMap.end()) {
		return false;
	}
	else {
		if (found->second->second.prob == 0 && found->second->second.backoff == ngram::kNoExtensionBackoff) {
			return false;
		}
		//cout << "GOOD JOB: " << key << " - " << found->second->second.prob;
		//cout << " - Entries: " << mEntries << endl;
		gram.prob = found->second->second.prob;
		gram.backoff = found->second->second.backoff;
	}
	return true;
}

} // namespace ngram
} // namespace cloudlm
