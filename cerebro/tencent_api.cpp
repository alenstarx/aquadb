#include <chrono>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <string.h>
#include <iostream>
#include <sstream>
#include <vector>

#include "util/common.h"
#include "util/logdef.h"
#include "tencent_api.h"


using namespace std;
using namespace std::chrono;
namespace tencent_api 
{


namespace detial {

void tokenize(std::vector<std::string>& tokens, const std::string& str, 
    const std::string& delim = " ");

void split(const string& s, const string& delim,
    vector<string>& ss, bool do_trim = true) ;

string trim(const std::string& src) {
    auto p1 = src.begin();
    //while (p1 != src.end() && isspace(*p1)) p1++;
    while (p1 != src.end() && *p1<=' ') p1++;
    if (p1 == src.end()) return "";
    
    auto p2 = p1 + (src.end() -  p1) - 1;
    //while (isspace(*p2) && p2>p1) p2--;
    while (*p2<= ' ' && p2>p1) p2--;

    return string(p1, p2+1);
}

void split(const string& s, const string& delim,
    vector<string>& ss, bool do_trim) {
    tokenize(ss, s, delim);
    if (do_trim) {
        for (size_t i = 0; i < ss.size(); i++) {
            ss[i] = trim(ss[i]);
        }
    }
}



int split(vector<string>& vs, const string& s) {
  tokenize(vs, s, " \t\n\r");  
  return vs.size();
}


vector<string> split(const string& s, const string& delim) {
  vector<string> vs;  
  tokenize(vs, s, delim);
  return vs;
}

int split(vector<string>& vs,  const string& s, const string& delim) {  
  tokenize(vs, s, delim);
  return vs.size();
}

vector<string> split(const string& s) {
  vector<string> vs;
  split(vs, s);
  return vs;
}

void tokenize(vector<string>& tokens, const string& str,
    const string& delim) {    
  size_t pre = 0;
  size_t pos = 0;
  
  do {
    pre = pos;
    while (pos < str.size() && delim.find(str[pos]) == string::npos) {
      ++pos;
    }
    tokens.push_back(str.substr(pre, pos - pre));
    ++pos;
  }while(pos < str.size());

  if(delim.find(str[pos - 1]) != string::npos){
    tokens.push_back("");
  }
}

std::string remove_substr(std::string str, const std::string& sub)
{
    size_t id = 0;
    size_t size = sub.size();
    id = str.find(sub, 0);
    //bool sub_found = false;
    while (id != std::string::npos) {
        str.erase(id, size);
        id = str.find(sub);
        //sub_found = true;
    }
    return str;
}


static inline int parse_date(const string& date)
{
	if (date.length() != 8) return 0;
	return atoi(date.c_str());
}

static inline int parse_time(const string& time)
{
	if (time.length() != 6) return 0;
	return atoi(time.c_str()) *1000;
}

static inline mdlink_api::MarketQuotePtr parse_quote(const string& raw_line)
{
    vector<string> kv;
    string line = remove_substr(raw_line, "\r");
    line = remove_substr(line, "\n");
    split(line, "=", kv);
    if (kv.size() != 2) {
        return nullptr;
    }
    auto quote = mdlink_api::make_quote_ptr();
    quote->bids.resize(5, 0.0);
    quote->asks.resize(5, 0.0);
    quote->bid_vols.resize(5, 0);
    quote->ask_vols.resize(5, 0);

    //memset(quote.get(), 0, sizeof(quote));

    {
        vector<string> tmp;
        split(kv[0], "_", tmp);
        string code = tmp.back();
        code = TencentApi::convert_to_extend_code(code);
    //    strncmp(quote->code, code.c_str(), 32);
        //strcpy(quote->code, code.c_str());   
        quote->symbol = std::move(code);
    }
    
    if (kv[1] == "\"\"") {
        return nullptr;
    }

    {
        vector<string> tmp;
        split(kv[1], "\"", tmp, false);

        string values_str = tmp[1];
        vector<string> values;

        split(values_str, "~", values);
        if (values.size() == 0) {
            return nullptr;
        }
		int i = 3;
		quote->last = atof(values[i++].c_str());
		quote->prev_close = atof(values[i++].c_str());
		quote->open = atof(values[i++].c_str());
		quote->volume = atoll(values[i++].c_str()) * 100;
		i += 2; // ignore the outer, inner

		quote->bids[0] = atof(values[i++].c_str());
		quote->bid_vols[0] = atoi(values[i++].c_str()) * 100;
		quote->bids[1] = atof(values[i++].c_str());
		quote->bid_vols[1] = atoi(values[i++].c_str()) * 100;
		quote->bids[2] = atof(values[i++].c_str());
		quote->bid_vols[2] = atoi(values[i++].c_str()) * 100;
		quote->bids[3] = atof(values[i++].c_str());
		quote->bid_vols[3] = atoi(values[i++].c_str()) * 100;
		quote->bids[4] = atof(values[i++].c_str());
		quote->bid_vols[4] = atoi(values[i++].c_str()) * 100;

		quote->asks[0] = atof(values[i++].c_str());
		quote->ask_vols[0] = atoi(values[i++].c_str()) * 100;
		quote->asks[1] = atof(values[i++].c_str());
		quote->ask_vols[1] = atoi(values[i++].c_str()) * 100;
		quote->asks[2] = atof(values[i++].c_str());
		quote->ask_vols[2] = atoi(values[i++].c_str()) * 100;
		quote->asks[3] = atof(values[i++].c_str());
		quote->ask_vols[3] = atoi(values[i++].c_str()) * 100;
		quote->asks[4] = atof(values[i++].c_str());
		quote->ask_vols[4] = atoi(values[i++].c_str()) * 100;
		i += 1; // ignore last transactions

		quote->dt = atoi(values[i].substr(0, 8).c_str()); // YYYYMMDD
		int32_t hhmmss = atoi(values[i++].substr(8, 6).c_str()); // hhmmss
        // fill ts
        quote->ts = quote->dt * 1000000L + hhmmss; 
		i += 2; // ignore ups and downs

        quote->high     = atof(values[i++].c_str());
        quote->low      = atof(values[i++].c_str());
        i += 2; // ignore transaction, vol

        quote->turnover = atoll(values[i++].c_str())*10000;

    }
    return quote;
}

}
TencentApi::TencentApi(int page_size)
{
    _page_size = page_size;
        _cli = new httplib::Client("http://qt.gtimg.cn");
    _cli->set_keep_alive(true);
    _cli->set_tcp_nodelay(true);
    _cli->set_follow_location(true);
    _cli->set_decompress(true);
    _cli->set_url_encode(false);
    _cli->set_default_headers(
        {
        {"Accept","text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8"},
        {"Accept-Language","zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2"},
        {"User-Agent","Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0"},
        {"Upgrade-Insecure-Requests","1"},
        {"Pragma","no-cache"},
        //{"Accept-Encoding", "gzip" }
        { "Accept-Encoding", "gzip, deflate" }
        }
    );
    //  _cli->set_proxy("192.168.53.99", 8888);

}

TencentApi::~TencentApi()
{
}

std::string TencentApi::convert_to_inner_code(const std::string& code)
{
    if(util::endswith(code, ".SH"))
    {
        return std::string("sh") + code.substr(0,6);
    }
    else if(util::endswith(code, ".SZ"))
    {
        return std::string("sz") + code.substr(0,6);
    }
    else {
        return code;
    }
}

std::string TencentApi::convert_to_extend_code(const std::string& code)
{
    if(util::startswith(code, "sh"))
    {
        return code.substr(2,6) + ".SH";
    }
    else if(util::startswith(code, "sz"))
    {
        return code.substr(2,6) + ".SZ";
    }
    else {
        return code;
    }
}


int TencentApi::get_quotes_by_url(const std::string& url, std::function<bool(mdlink_api::MarketQuotePtr)> fn)
{
    std::string body;

    auto res = _cli->Get(url.c_str());
    if(res.error() != httplib::Error::Success)
    {
        // TODO
        auto err = res.error();
        std::cerr << "status=" << res->status << ",error=" << err << ",url=" << url << std::endl; 
        return static_cast<int>(err);
    }
    else if(res->status == 200)
    {
        //std::cout << ">url=" << url << std::endl;
        //std::cout << ">header=" << util::to_string(res->headers.begin(), res->headers.end())<< std::endl;
        //std::cout << ">body.size=" << res->body.size() <<"\n" << res->body << std::endl;
        body = std::move(res->body);
    }
    else 
    {
        // TODO
        auto err = res.error();
        std::cerr << "status=" << res->status << ",error=" << err << ",url=" << url << std::endl; 
        return static_cast<int>(err);
    }


    vector<string> lines;
    detial::split(body, ";", lines);
    for (auto const& line : lines) {
        if (strncmp(line.c_str(), "v", 1) != 0) continue;        
        auto q = detial::parse_quote(detial::trim(line));
        if (q) {
            //quotes->push_back(q);
            // TODO
            if(!fn(q))
            {
                break;
            }
        }
        else 
        {
            LOGE("invalid data:%s", line.c_str());
        }
    }
    return 0;
}

int TencentApi::get_quotes(const std::vector<std::string>& codes, std::function<bool(mdlink_api::MarketQuotePtr)> fn)
{
    size_t page_num = (codes.size() + _page_size -1) / (_page_size);
    for(size_t n =0; n < page_num; ++n)
    {
    std::stringstream ss;
    ss << "/q=";
    for (size_t i = 0; i < _page_size; i++) {
        ss << convert_to_inner_code(codes.at(n * _page_size+ i));
        if((n * _page_size + i + 1) == codes.size())
        {
            break;
        }
        if (i + 1 != _page_size)
            ss << ",";
    }
    std::string url = ss.str();
    get_quotes_by_url(url, fn);
    }
    return 0;
}

}