#pragma once

#include "hmac_sha2.h"

#include "cpprest/json.h"

#include <iomanip>
#include <string>
#include <unordered_map>
#ifdef _WIN32
#include <codecvt>
#endif

namespace tylawin
{
	namespace CppRest
	{
		namespace Utilities
		{
			typedef std::unordered_map<std::string, std::string> QueryParams;

#ifdef _WIN32
			std::wstring s2ws(const std::string &str)
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wstrConvert;

				return wstrConvert.from_bytes(str);
			}

			std::string ws2s(const std::wstring &wstr)
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> wstrConvert;

				return wstrConvert.to_bytes(wstr);
			}

			utility::string_t s2u(const std::string &str)
			{
				return s2ws(str);
			}

			std::string u2s(const utility::string_t &ustr)
			{
				return ws2s(ustr);
			}
#else
			utility::string_t s2u(const std::string &str)
			{
				return str;
			}

			std::string u2s(const utility::string_t &ustr)
			{
				return ustr;
			}
#endif

			std::string paramsToUrlString(const QueryParams &m) {
				std::string res("");
				for(QueryParams::const_iterator iter = m.begin(); iter != m.end(); ++iter)
				{
					if(iter != m.begin()) res += "&";
					res += iter->first + "=" + iter->second;
				}
				return res;
			}

			std::string paramUriToValidFileName(const std::string &uri)
			{
				std::string path;

				for(auto ch : uri)
				{
					if(ch == '/' || ch == '\\' || ch == '?')
						path += '-';
					else if(ch == '+')
						path += ' ';
					else if(ch == '=')
						path += '_';
					else if(ch != '&')
						path += ch;
				}

				return path;
			}

			std::string convertJsonValueToFormattedString(const web::json::value &v, uint32_t indent_level = 0)
			{
				std::string str;

				if(v.is_object())
				{
					str += "{";
					++indent_level;

					auto va = v.as_object();
					for(auto iter = va.cbegin(); iter != va.cend(); ++iter)
					{
						if(iter != va.cbegin())
							str += ",";

						str += "\n";
						for(size_t i = 0; i < indent_level; ++i)
							str += "\t";
#ifdef _WIN32
						str += ws2s(iter->first) + ":";
#else
						str += iter->first + ":";
#endif
						str += convertJsonValueToFormattedString(iter->second, indent_level);
					}
					str += "\n";

					--indent_level;
					for(size_t i = 0; i < indent_level; ++i)
						str += "\t";
					str += "}";
				}
				else if(v.is_array())
				{
					str += "[";
					++indent_level;

					auto va = v.as_array();
					for(auto iter = va.cbegin(); iter != va.cend(); ++iter)
					{
						if(iter != va.cbegin())
							str += ",";

						str += "\n";
						for(size_t i = 0; i < indent_level; ++i)
							str += "\t";
						str += convertJsonValueToFormattedString(*iter, indent_level);
					}
					str += "\n";

					--indent_level;
					for(size_t i = 0; i < indent_level; ++i)
						str += "\t";
					str += "]";
				}
				else
				{
#ifdef _WIN32
					str += ws2s(v.serialize());
#else
					str += v.serialize();
#endif
				}

				return str;
			}

			std::string hmacSha512(const std::string &key, const std::string &message)
			{
				unsigned char digest[SHA512_DIGEST_SIZE];

				hmac_sha512(reinterpret_cast<const unsigned char*>(key.data()), static_cast<unsigned int>(key.size())
					, reinterpret_cast<const unsigned char*>(message.data()), static_cast<unsigned int>(message.size())
					, digest, SHA512_DIGEST_SIZE);
				
				/*TODO: strstream instead of char []
				std::ostringstream tmpStrStream;
				tmpStrStream << std::hex << std::setfill('0');
				for(size_t i = 0; i < SHA512_DIGEST_SIZE; ++i)
				{
					tmpStrStream << std::setw(2) << digest[i];
				}*/
				
				char messageDigest[2 * SHA512_DIGEST_SIZE + 1];
				for(int i = 0; i < SHA512_DIGEST_SIZE; ++i)
				{
					snprintf(messageDigest + i * 2, 3, "%02x", static_cast<unsigned int>(digest[i]));
				}

				return std::string(messageDigest);
				
				//return tmpStrStream.str();
			}
		}
	}
}