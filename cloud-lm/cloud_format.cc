#include "cloud-lm/cloud_format.hh"
#include "cloud-lm/config.hh"

#include "cloud-lm/blank.hh"
#include "cloud-lm/lm_exception.hh"

#include "cloud-lm/timer.hh"

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
//#include <boost/regex.hpp>

using namespace std;

namespace cloudlm {
namespace ngram {

Timer cacheTime;
Timer networkTime;
Timer networkTimeOpenSocket;
Timer networkTimeSolr;
Timer prepareRequestsTime;
Timer splitPhrasesTimer;
double solrTime = 0; // In ms
int solrRequests = 0;
int cacheRequests = 0;
int cacheAdd = 0;
int cacheExist = 0;
int cacheGet = 0;
int cacheFound = 0;
int cacheNotFound = 0;

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

///////// SOCKET //////////
ClientSocket::ClientSocket():
		io_service_(), socket_(io_service_), is_setup(false) {}


void ClientSocket::OpenConnection(std::string ip, std::string port) {
	boost::asio::ip::tcp::resolver resolver(io_service_);
	boost::asio::ip::tcp::resolver::query query(ip,port);
	iterator_ = resolver.resolve(query);
	//boost::asio::ip::tcp::endpoint endpoint = *iterator;
	//socket_.async_connect(endpoint, boost::bind(&ClientSocket::HandlerConnect, this, boost::asio::placeholders::error, iterator));
	//io_service_.run();

	is_setup = true;
}

//void ClientSocket::HandlerConnect(const boost::system::error_code &error, boost::asio::ip::tcp::resolver::iterator endpoint_iterator) {
//	if(error)
//	{
//		boost::system::error_code error = boost::asio::error::host_not_found;
//		throw boost::system::system_error(error);
//	}
//	else
//	{
//		cout << "TEST2" << endl;
//		boost::asio::socket_base::keep_alive keep_option(true);
//		socket_.set_option(keep_option);
//	}
//}

//void ClientSocket::HandleWriteRequest(const boost::system::error_code& err) {
//	if (!err)
//	{
//		cout << "TEST3.1" << endl;
//	  // Read the response status line. The response_ streambuf will
//	  // automatically grow to accommodate the entire line. The growth may be
//	  // limited by passing a maximum size to the streambuf constructor.
//	  boost::asio::async_read_until(socket_, response_, "\r\n",
//		  boost::bind(&ClientSocket::HandleReadStatusLine, this,
//			boost::asio::placeholders::error));
//	}
//	else
//	{
//	  std::cout << "Error: " << err.message() << "\n";
//	}
//}
//
//void ClientSocket::HandleReadStatusLine(const boost::system::error_code& err) {
//    if (!err)
//    {
//		cout << "TEST3.2" << endl;
//      // Check that response is OK.
//      std::istream response_stream(&response_);
//      std::string http_version;
//      response_stream >> http_version;
//      unsigned int status_code;
//      response_stream >> status_code;
//      std::string status_message;
//      std::getline(response_stream, status_message);
//      if (!response_stream || http_version.substr(0, 5) != "HTTP/")
//      {
//        std::cout << "Invalid response\n";
//        return;
//      }
//      if (status_code != 200)
//      {
//        std::cout << "Response returned with status code ";
//        std::cout << status_code << "\n";
//        return;
//      }
//
//      // Read the response headers, which are terminated by a blank line.
//      boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
//          boost::bind(&ClientSocket::HandleReadHeaders, this,
//            boost::asio::placeholders::error));
//    }
//    else
//    {
//      std::cout << "Error: " << err << "\n";
//    }
//}
//
//void ClientSocket::HandleReadHeaders(const boost::system::error_code& err) {
//    if (!err)
//    {
//		cout << "TEST3.3" << endl;
//      // Process the response headers.
//      std::istream response_stream(&response_);
//      std::string header;
//      while (std::getline(response_stream, header) && header != "\r")
//        std::cout << header << "\n";
//      std::cout << "\n";
//
//      // Write whatever content we already have to output.
//      if (response_.size() > 0)
//        std::cout << &response_;
//
//      // Start reading remaining data until EOF.
//      boost::asio::async_read(socket_, response_,
//          boost::asio::transfer_at_least(1),
//          boost::bind(&ClientSocket::HandleReadContent, this,
//            boost::asio::placeholders::error));
//    }
//    else
//    {
//      std::cout << "Error: " << err << "\n";
//    }
//}
//
//void ClientSocket::HandleReadContent(const boost::system::error_code& err) {
//    if (!err)
//    {
//		cout << "TEST3.4" << endl;
//      // Write all of the data that has been read so far.
//      //std::cout << &response_;
//      solr_response << &response_;
//
//      // Continue reading remaining data until EOF.
//      boost::asio::async_read(socket_, response_,
//          boost::asio::transfer_at_least(1),
//          boost::bind(&ClientSocket::HandleReadContent, this,
//            boost::asio::placeholders::error));
//    }
//    else if (err != boost::asio::error::eof)
//    {
//      std::cout << "Error: " << err << "\n";
//    }
//}

void ClientSocket::SendRequest(string message, string url, stringstream &solr_response) {
	try {
		cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();

		networkTimeOpenSocket.start();

		tcp::resolver::iterator end;
		tcp::resolver::iterator iterator = iterator_;

		// Try each endpoint until we successfully establish a connection.
		boost::system::error_code error = boost::asio::error::host_not_found;
		while (error && iterator != end)
		{
		  socket_.close();
		  socket_.connect(*iterator++, error);
		  boost::asio::socket_base::keep_alive keep_option(true);
		  socket_.set_option(keep_option);
		}
		if (error) {
		  networkTimeOpenSocket.stop();
		  throw boost::system::system_error(error);
		}
		networkTimeOpenSocket.stop();

		networkTimeSolr.start();
		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		boost::asio::streambuf request;
		ostream request_stream(&request);
		//request_stream << "GET " << "/solr/collection1/select?q=gram:\"" << UriEncode(req.gram) << "\"&fq=order:" << req.order << "&wt=json" << " HTTP/1.0\r\n";
		request_stream << "GET " << "/solr/collection1/select?wt=json&fl=gram,prob,backoff&" << message << " HTTP/1.1\r\n";
		request_stream << "Host: " << config->request.url << "\r\n";
		request_stream << "Accept: */*;charset=UTF-8\r\nAccept-Charset: UTF-8\r\nConnection: close\r\n\r\n";

		// Send the request.
		boost::asio::write(socket_, request);

		// Read the response status line. The response streambuf will automatically
		// grow to accommodate the entire line. The growth may be limited by passing
		// a maximum size to the streambuf constructor.
		boost::asio::streambuf response;
		//boost::asio::read_until(socket, response, "\r\n");
		// Read until EOF, writing data to output as we go.
		while (boost::asio::read(socket_, response,
			  boost::asio::transfer_at_least(1), error)) {}
		  //cout << &response;
		if (error != boost::asio::error::eof) {
		  networkTime.stop();
		  throw boost::system::system_error(error);
		}
		networkTimeSolr.stop();
		networkTime.stop();

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
		  cerr << "Invalid response for the search: " << message << endl;
		  return;
		}
		if (status_code != 200)
		{
		  // TODO: change the failed requests
		  //throw boost::system::system_error(boost::asio::error::connection_aborted);
		  cerr << "Response returned with status code " << status_code << " when searched: " << message << endl;
		  return;
		}

		// Read the response headers, which are terminated by a blank line.
		//boost::asio::read_until(socket, response, "\r\n\r\n");

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
		UTIL_THROW(SolrException, "Solr request - we can't connect with Solr. Reason:\n" << e.what());
		return;
	}
}
///////// END SOCKET //////////

ClientSocket clientSocket;

/**
 * Action = 0 -> start / Action = 1 -> stop
 */
void doSplitPhrasesTimer(int action) {
	if (action == 0) {
		splitPhrasesTimer.start();
	}
	else if (action == 1) {
		splitPhrasesTimer.stop();
	}
}

void UpdateRequestStats(const int order) {
    hist_order[order] = hist_order[order] + 1;
}

void UpdateRequestStats(const string gram, const int order) {
    hist_word[gram] = hist_word[gram] + 1;
    hist_order[order] = hist_order[order] + 1;
}

void ShowStats() {
	cerr << endl << endl << "START STATS" << endl;
	/*cerr << "--NGRAMS--" << endl;

	BOOST_FOREACH(HistogramWord::value_type i, hist_word) {
	    cerr << i.first << "\t" << i.second << endl;
	}*/

	cerr << endl << endl << "--ORDER--" << endl;
	BOOST_FOREACH(HistogramOrder::value_type i, hist_order) {
	    cerr << i.first << "\t" << i.second << endl;
	}

	cerr << endl << endl << "--TIME--" << endl;
	cerr << "cacheTime" << "\t" << cacheTime.get_elapsed_time() << endl;
	cerr << "networkTime" << "\t" << networkTime.get_elapsed_time() << endl;
	cerr << "networkTimeSolr" << "\t" << networkTimeSolr.get_elapsed_time() << endl;
	cerr << "prepareRequestsTime" << "\t" << prepareRequestsTime.get_elapsed_time() << endl;
	cerr << "splitPhrasesTimer" << "\t" << splitPhrasesTimer.get_elapsed_time() << endl;
	cerr << "solrTime" << "\t" << solrTime << endl;
	cerr << "solrRequests" << "\t" << solrRequests << endl;
	cerr << "cacheRequests" << "\t" << cacheRequests << endl;
	cerr << "cacheAdd" << "\t" << cacheAdd << endl;
	cerr << "cacheExist" << "\t" << cacheExist << endl;
	cerr << "cacheGet" << "\t" << cacheGet << endl;
	cerr << "cacheFound" << "\t" << cacheFound << endl;
	cerr << "cacheNotFound" << "\t" << cacheNotFound << endl;
	cerr << "mMaxEntries" << "\t" << mMaxEntries << endl;
	cerr << "mEntries" << "\t" << mEntries << endl;

	cerr << endl << endl << "FINISH STATS" << endl;
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
  prepareRequestsTime.start();
  solrRequests++;
  try
  {
	  cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
	  networkTime.start();
	  if (!clientSocket.is_setup) {
		  clientSocket.OpenConnection(config->request.base_url, config->request.port);
	  }
	  networkTimeSolr.start();
	  clientSocket.SendRequest(search, config->request.url, solr_response);
	  networkTimeSolr.stop();
	  networkTime.stop();
  }
  catch (exception& e)
  {
	UTIL_THROW(SolrException, "Solr request - we can't connect with Solr. Reason:\n" << e.what());
	prepareRequestsTime.stop();
	return;
  }
  prepareRequestsTime.stop();
}

 void SendRequestSolr2(string search, stringstream &solr_response) {
  prepareRequestsTime.start();
  solrRequests++;

  try
  {
    cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();

	boost::asio::io_service io_service;

	networkTime.start();

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
	if (error) {
	  networkTime.stop();
	  throw boost::system::system_error(error);
	}

	networkTimeSolr.start();
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
	if (error != boost::asio::error::eof) {
	  networkTime.stop();
	  throw boost::system::system_error(error);
	}
	networkTimeSolr.stop();
	networkTime.stop();

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
	  cerr << "Invalid response for the search: " << search << endl;
	  prepareRequestsTime.stop();
	  return;
	}
	if (status_code != 200)
	{
	  // TODO: change the failed requests
	  //throw boost::system::system_error(boost::asio::error::connection_aborted);
	  cerr << "Response returned with status code " << status_code << " when searched: " << search << endl;
	  prepareRequestsTime.stop();
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
	UTIL_THROW(SolrException, "Solr request - we can't connect with Solr. Reason:\n" << e.what());
	prepareRequestsTime.stop();
	return;
  }
  prepareRequestsTime.stop();
}

bool SendRequest(Data &req, ProbBackoff &gram) {
	if (GetFromCache(req.gram, gram)) return true;

	stringstream query;
	query << "q=gram:\"" << UriEncode(EncodeSolrSpecialCharacters(req.gram)) << "\"&fq=order:" << req.order;

	stringstream response;
	SendRequestSolr(query.str(), response);
	bool gram_found = ReadJson(response, gram);
	// TODO: We can apply this step on Solr side. Remove the condition when we change on Solr
	if (!gram_found && gram.backoff == ngram::kExtensionBackoff) gram.backoff = ngram::kNoExtensionBackoff;
	AddToCache(req.gram, gram);
	if (!gram_found) return false;

	return true;
}

void SendRequest(vector<string> words) {
	// TODO: we put the not found words / phrases in the cache --- WE CAN'T PUT for the moment
	cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
	int expected_results = 0;
	for (int n = 1; n <= config->max_order; n++) { // for each n-gram order
		splitPhrasesTimer.start();
		string search_per_order = "";
		signed int limit = words.size() - n + 1;
		// get all the possible n-grams for this order
		for (int i = 0; i < limit; i++) {
			string search_per_gram = "";
			for (int j = i; j < i + n; j++) {
				search_per_gram += (j > i ? " " : "") + words[j];
			}
			if (search_per_gram != "" && !ExistInCache(search_per_gram)){
				search_per_order += "gram:\"" + EncodeSolrSpecialCharacters(search_per_gram) + "\" ";
				expected_results++;
			}
		}
		splitPhrasesTimer.stop();
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

// TODO: Deprecated function
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
				search_per_order += "gram:\"" + EncodeSolrSpecialCharacters(search_per_gram) + "\" ";
				expected_results++;
			}
		}
		if (search_per_order != "") {
			search << "q=" << UriEncode(search_per_order) << "&fq=order:" << n << "";
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

		solrTime += pt.get<double>("responseHeader.QTime", 0);

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
	boost::property_tree::ptree pt;
	boost::property_tree::read_json(json, pt);

	solrTime += pt.get<double>("responseHeader.QTime", 0);

	int numFound = pt.get<int>("response.numFound", 0);

	if (numFound == 0) return false;
	BOOST_FOREACH(boost::property_tree::ptree::value_type &v, pt.get_child("response.docs."))
	{
		//assert(v.first.empty()); // array elements have no names
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
	return true;
}

void AddToCache(string key, const ProbBackoff value) {

    cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
	if (config->cache == 0) return;

	cacheTime.start();
	cacheRequests++;
	cacheAdd++;

	// push it to the front;
	mCacheList.push_front( make_pair( key, value ) );

	// add it to the cache map
	mCacheMap[ key ] = mCacheList.begin();

	// increase count of entries
	mEntries++;

	// check if it's time to remove the last element
	if ( mEntries > mMaxEntries )
	{
		cerr << "CLOUDLM WARN: The limit of the cache (" << mMaxEntries << ") is reached. Current size: " << mEntries << endl;

	    // erase from the map the last cache list element
	    mCacheMap.erase( mCacheList.back().first );

	    // erase it from the list
	    mCacheList.pop_back();

	    // decrease count
	    mEntries--;
	}

	cacheTime.stop();
}



bool ExistInCache(const string key) {
	cacheTime.start();
	cacheRequests++;
	cacheExist++;

	CacheMap::const_iterator found = mCacheMap.find(key);
	if (found == mCacheMap.end()) {
		cacheTime.stop();
		return false;
	}
	else {
		cacheTime.stop();
		return true;
	}

}

bool GetFromCache(const string key, ProbBackoff &gram) {
    cloudlm::ngram::Config *config = cloudlm::ngram::Config::Instance();
	if (config->cache == 0) return false;

	cacheTime.start();
	cacheRequests++;
	cacheGet++;

	CacheMap::const_iterator found = mCacheMap.find(key);
	if (found == mCacheMap.end()) {
		cacheNotFound++;
		cacheTime.stop();
		return false;
	}
	else {
		if (found->second->second.prob == 0 && found->second->second.backoff == ngram::kNoExtensionBackoff) {
			cacheNotFound++;
			cacheTime.stop();
			return false;
		}
		//cout << "GOOD JOB: " << key << " - " << found->second->second.prob;
		//cout << " - Entries: " << mEntries << endl;
		gram.prob = found->second->second.prob;
		gram.backoff = found->second->second.backoff;
	}
	cacheFound++;
	cacheTime.stop();
	return true;
}

} // namespace ngram
} // namespace cloudlm
