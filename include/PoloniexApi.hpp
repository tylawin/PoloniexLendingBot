/*
Copyright 2016 Tyler Winters

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#include "cpprest_utilities.hpp"
#include "Decimal.hpp"

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>

#ifdef _WIN32
#include <filesystem>
namespace filesystem = std::experimental::filesystem;
#else
#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;
#endif

#include <string>
#include <thread>
#include <map>
#include <unordered_map>

namespace tylawin
{
	namespace poloniex
	{
		typedef std::string CurrencyCode;

		typedef DataTypes::Decimal Amount;
		typedef DataTypes::Decimal Rate;

		class PoloniexApi
		{
		public:
			typedef uint64_t OrderNumber;

			static constexpr long double minimumRateIncrement_ = 0.000001L;

			PoloniexApi(const std::string &key, const std::string &secret) :
				key_(key),
				secret_(secret),
				nonce_(0)
			{
				httpClient = new web::http::client::http_client(web::uri(U("https://poloniex.com")));
				if(filesystem::exists("nonce.txt"))
				{
					std::ifstream f("nonce.txt");
					f >> nonce_;
					f.close();
				}
			}

			~PoloniexApi()
			{
				delete httpClient;

				saveNonce();
			}

		private://noncopyable
			PoloniexApi(const PoloniexApi &) = delete;
			PoloniexApi& operator=(const PoloniexApi &) = delete;

		public:
			struct CancelLoanOfferResponse
			{
				bool success_;
				std::string msg_;
			};
			CancelLoanOfferResponse cancelLoanOffer(const OrderNumber &orderNumber)
			{
				auto jsonResponse = query(web::http::methods::POST, true, "/tradingApi", { {"command","cancelLoanOffer"}, {"orderNumber",std::to_string(orderNumber)} });
				CancelLoanOfferResponse response;
				response.msg_ = "ERROR expected json response field missing";
				if(!jsonResponse.has_field(U("success")) || jsonResponse[U("success")].as_integer() == false)
				{
					response.success_ = false;
					if(jsonResponse.has_field(U("error")))
						response.msg_ = CppRest::Utilities::u2s(jsonResponse[U("error")].as_string());
				}
				else
				{
					response.success_ = true;
					if(jsonResponse.has_field(U("message")))
						response.msg_ = CppRest::Utilities::u2s(jsonResponse[U("message")].as_string());
				}
				return response;
			}

			auto createLoanOffer(const std::string &currency, const std::string &amount, const uint8_t &maxDurationDays, bool autoRenew, const std::string &lendingRate)
			{
				if(maxDurationDays < 2 || maxDurationDays > 60)
					throw std::runtime_error("Invalid argument(duration:" + std::to_string(maxDurationDays) + "). Poloniex duration range is [2,60].");
				return query(web::http::methods::POST, true, "/tradingApi", { {"command","createLoanOffer"}, {"currency",currency}, {"amount",amount}, {"duration",std::to_string(maxDurationDays)}, {"autoRenew",std::to_string(autoRenew)}, {"lendingRate",lendingRate} });
			}

			typedef size_t LoanId;
			struct ActiveLoan
			{
				LoanId id_;
				Amount amount_;
				Rate rate_;
				uint16_t duration_;
				bool autoRenew_;
				boost::posix_time::ptime dateTime_;
				Amount fees_;
			};
			typedef std::unordered_map<CurrencyCode, std::unordered_map<LoanId, ActiveLoan>> ActiveLoans;
			ActiveLoans getActiveLoans()
			{
				auto jsonResponse = query(web::http::methods::POST, true, "/tradingApi", { {"command","returnActiveLoans"} });

				ActiveLoans activeLoans;

				if(jsonResponse.has_field(U("provided")))
				{
					auto providedLoans = jsonResponse[U("provided")].as_array();

					for(auto loan : providedLoans)
					{
						CurrencyCode curCode;
						ActiveLoan activeLoan;

						curCode = CppRest::Utilities::u2s(loan[U("currency")].as_string());

						activeLoan.id_        = loan[U("id")].as_integer();
						activeLoan.amount_    = CppRest::Utilities::u2s(loan[U("amount")].as_string());
						activeLoan.rate_      = CppRest::Utilities::u2s(loan[U("rate")].as_string());
						activeLoan.duration_  = loan[U("duration")].as_integer();
						activeLoan.autoRenew_ = loan[U("autoRenew")].as_integer() != 0;
						activeLoan.dateTime_  = boost::posix_time::time_from_string(CppRest::Utilities::u2s(loan[U("date")].as_string()));
						activeLoan.fees_      = CppRest::Utilities::u2s(loan[U("fees")].as_string());

						activeLoans[curCode].insert(std::make_pair(activeLoan.id_, activeLoan));
					}
				}

				return activeLoans;
			}

			enum class AccountTypes
			{
				EXCHANGE,
				MARGIN,
				LENDING
			};
			//TODO: remove when rpi gcc updates: enum class automatic hash doesn't work in gcc 4.9 (raspbian)
			struct AccountTypesHash
			{
				template<typename T>
				std::size_t operator()(T t) const
				{
					return static_cast<std::size_t>(t);
				}
			};
			typedef std::unordered_map<AccountTypes, std::unordered_map<CurrencyCode, Amount>, AccountTypesHash> AccountBalances;
			auto getAvailableAccountBalances(const boost::optional<AccountTypes> accountType = boost::none)
			{
				web::json::value response;

				if(!accountType)
					response = query(web::http::methods::POST, true, "/tradingApi", { {"command","returnAvailableAccountBalances"} });
				else
				{
					std::string type;
					switch(*accountType)
					{
						case AccountTypes::EXCHANGE: type = "exchange"; break;
						case AccountTypes::MARGIN:   type = "margin";   break;
						case AccountTypes::LENDING:  type = "lending";  break;
						default: throw std::runtime_error("invalid accountType enum");
					}
					response = query(web::http::methods::POST, true, "/tradingApi", { {"command","returnAvailableAccountBalances"}, {"account",type} });
				}

				AccountBalances accountBalances;
				accountBalances[AccountTypes::EXCHANGE] = std::unordered_map<CurrencyCode, Amount>();
				accountBalances[AccountTypes::MARGIN] = std::unordered_map<CurrencyCode, Amount>();
				accountBalances[AccountTypes::LENDING] = std::unordered_map<CurrencyCode, Amount>();
				if (response.size() == 0)
					;
				else
				{
					for (auto accountTypeBalances : response.as_object())
					{
						auto accountTypeStr = CppRest::Utilities::u2s(accountTypeBalances.first);
						AccountTypes accountType;
						if (accountTypeStr == "exchange")
							accountType = AccountTypes::EXCHANGE;
						else if (accountTypeStr == "margin")
							accountType = AccountTypes::MARGIN;
						else if (accountTypeStr == "lending")
							accountType = AccountTypes::LENDING;
						else
							continue;//skip unknown type

						if(accountTypeBalances.second.size() > 0)
							for (auto balance : accountTypeBalances.second.as_object())
							{
								CurrencyCode curCode = CppRest::Utilities::u2s(balance.first);
								Amount amt = CppRest::Utilities::u2s(balance.second.as_string());

								accountBalances[accountType][curCode] = amt;
							}
					}
				}
				return accountBalances;
			}

			
			struct LoanOrders
			{
				struct Details
				{
					Amount amount_;
					uint16_t rangeMin_, rangeMax_;
				};
				typedef std::multimap<Rate, Details> Offers;
				Offers offers_;
				typedef std::multimap<Rate, Details> Demands;
				Demands demands_;
			};
			LoanOrders getLoanOrders(const std::string &currency, const boost::optional<uint16_t> limit = boost::none)
			{
				web::json::value response;
				if(!limit)
					response = query(web::http::methods::GET, false, "/public", { {"command","returnLoanOrders"}, {"currency",currency} });
				else
					response = query(web::http::methods::GET, false, "/public", { {"command","returnLoanOrders"}, {"currency",currency}, {"limit",std::to_string(*limit)} });

				if(response.has_field(U("error")))
					throw std::runtime_error("error reponse: " + CppRest::Utilities::u2s(response[U("error")].as_string()));

				LoanOrders loanOrders;
				LoanOrders::Details tmpDetails;
				if(response[U("offers")].size() != 0)
				{
					for(auto offer : response[U("offers")].as_array())
					{
						tmpDetails.amount_ = CppRest::Utilities::u2s(offer[U("amount")].as_string());
						tmpDetails.rangeMin_ = offer[U("rangeMin")].as_integer();
						tmpDetails.rangeMax_ = offer[U("rangeMax")].as_integer();

						loanOrders.offers_.insert(std::make_pair(CppRest::Utilities::u2s(offer[U("rate")].as_string()), tmpDetails));
					}
				}
				if(response[U("demands")].size() != 0)
				{
					for(auto offer : response[U("demands")].as_array())
					{
						tmpDetails.amount_ = CppRest::Utilities::u2s(offer[U("amount")].as_string());
						tmpDetails.rangeMin_ = offer[U("rangeMin")].as_integer();
						tmpDetails.rangeMax_ = offer[U("rangeMax")].as_integer();

						loanOrders.demands_.insert(std::make_pair(CppRest::Utilities::u2s(offer[U("rate")].as_string()), tmpDetails));
					}
				}

				return loanOrders;
			}

			struct LoanOffer
			{
				LoanId id_;
				Amount amount_;
				Rate rate_;
				uint16_t duration_;
				bool autoRenew_;
				boost::posix_time::ptime date_;
			};
			typedef std::unordered_map<CurrencyCode, std::vector<LoanOffer>> LoanOffers;
			auto getOpenLoanOffers()
			{
				auto response = query(web::http::methods::POST, true, "/tradingApi", { { "command","returnOpenLoanOffers" } });
				if(response.has_field(U("error")))
				{
					throw std::runtime_error("error response: " + CppRest::Utilities::u2s(response[U("error")].as_string()));
				}

				LoanOffers loanOffers;
				if (response.size() != 0)
				{
					for (auto cur : response.as_object())
					{
						CurrencyCode loanCurCode = CppRest::Utilities::u2s(cur.first);
						for (auto offer : response[CppRest::Utilities::s2u(loanCurCode)].as_array())
						{
							loanOffers[loanCurCode].emplace_back(LoanOffer({
								static_cast<LoanId>(offer[U("id")].as_integer()),
								CppRest::Utilities::u2s(offer[U("amount")].as_string()),
								CppRest::Utilities::u2s(offer[U("rate")].as_string()),
								static_cast<uint16_t>(offer[U("duration")].as_integer()),
								offer[U("autoRenew")].as_integer() != 0,
								boost::posix_time::time_from_string(CppRest::Utilities::u2s(offer[U("date")].as_string()))
							}));
						}
					}
				}

				return loanOffers;
			}

			auto toggleAutoRenew(OrderNumber orderNumber)
			{
				return query(web::http::methods::POST, true, "/tradingApi", { {"command","toggleAutoRenew"}, {"orderNumber",std::to_string(orderNumber)} });
			}

		private:
			std::string key_;
			std::string secret_;
			uint64_t nonce_;
			web::http::client::http_client *httpClient;

			void saveNonce()
			{
				std::ofstream f("nonce.txt", std::ofstream::trunc);
				f << nonce_;
				f.flush();
				f.close();
			}

			web::http::http_request makeRequest(web::http::method method, bool authenticated, const std::string &path, CppRest::Utilities::QueryParams params = CppRest::Utilities::QueryParams())
			{
				web::http::http_request request;

				request.set_method(method);

				std::string urlEncodedParameters = "";
				if (authenticated)
				{
					params["nonce"] = std::to_string(++nonce_);
					saveNonce();
					urlEncodedParameters = CppRest::Utilities::paramsToUrlString(params);
					std::string signedUrlEncodedParameters = CppRest::Utilities::hmacSha512(secret_, urlEncodedParameters);
					request.headers().add(U("Content-Type"), U("application/x-www-form-urlencoded"));
					request.headers().add(U("Sign"), CppRest::Utilities::s2u(signedUrlEncodedParameters));
					request.headers().add(U("Key"), CppRest::Utilities::s2u(key_));
					request.headers().add(U("Content-Length"), CppRest::Utilities::s2u(std::to_string(urlEncodedParameters.size())));
				}
				else
					urlEncodedParameters = CppRest::Utilities::paramsToUrlString(params);

				std::string reqUri = path;
				if (!authenticated && params.size() > 0)
					reqUri += "?" + urlEncodedParameters;
				else if (authenticated)
					request.set_body(urlEncodedParameters);
				request.set_request_uri(CppRest::Utilities::s2u(reqUri));

				return request;
			}

			void writeQueryDebugOutputFile(const web::http::http_request &request, bool authenticated, const CppRest::Utilities::QueryParams &params, const web::json::value &res_json)
			{
				std::string fileName = "logs/query_" + CppRest::Utilities::paramUriToValidFileName(CppRest::Utilities::u2s(request.request_uri().to_string()));
				if (authenticated)
					fileName += "-" + CppRest::Utilities::paramUriToValidFileName(CppRest::Utilities::paramsToUrlString(params));
				fileName += ".txt";
				filesystem::path p(fileName);
				if (!filesystem::exists(p.parent_path()))
					filesystem::create_directories(p.parent_path());
				utility::ofstream_t of(p.string());
				if (of.is_open() == false)
				{
					throw std::runtime_error("Unable to open file(" + fileName + ")");
				}
				else
				{
					of << U("--REQUEST--") << std::endl;
					of << request.to_string() << std::endl << U("--RESPONSE--") << std::endl << std::endl;
					of << CppRest::Utilities::s2u(CppRest::Utilities::convertJsonValueToFormattedString(res_json));
					of.close();
				}
			}

			web::json::value query(web::http::method method, bool authenticated, const std::string &path, CppRest::Utilities::QueryParams params = CppRest::Utilities::QueryParams(), bool outputDebugFile = false)
			{
				bool retry = true;
				while(retry)
				{
					web::http::http_request request = makeRequest(method, authenticated, path, params);

					//Request rate limit: 6 per second max
					const static std::chrono::milliseconds minRequestRateLimitTime(1000 / 6);
					static std::chrono::milliseconds requestRateLimitTime = minRequestRateLimitTime;

					static std::chrono::time_point<std::chrono::steady_clock> lastTime = std::chrono::steady_clock::now() - requestRateLimitTime;
					auto now = std::chrono::steady_clock::now();
					if(now - lastTime < requestRateLimitTime)
						std::this_thread::sleep_for(requestRateLimitTime - (now - lastTime));
					lastTime = std::chrono::steady_clock::now();

					retry = false;
					try
					{
						
						web::json::value ret = httpClient->request(request).then([](web::http::http_response response) -> auto
						{
							if(response.status_code() == web::http::status_codes::OK && response.headers().content_type().substr(0, utility::string_t(U("application/json")).size()) == U("application/json"))
							{
								return response.extract_json();
							}
							else if(response.headers().content_type().substr(0, utility::string_t(U("text/html")).size()) == U("text/html"))
							{
								auto atask = response.extract_string();
								try
								{
									auto str = atask.get();

									if (requestRateLimitTime > minRequestRateLimitTime)
									{
										requestRateLimitTime *= 0.99;
										if (requestRateLimitTime < minRequestRateLimitTime)
											requestRateLimitTime = minRequestRateLimitTime;
									}

									return pplx::task_from_result<web::json::value>(web::json::value(str));
								}
								catch(const std::exception &e)
								{
									std::cout << __FILE__ ":" << __LINE__ << " - except: " << e.what() << std::endl;
									return pplx::task_from_result<web::json::value>(web::json::value());
								}
								throw std::runtime_error("error: unexpected response (" + std::to_string(response.status_code()) + ": " + CppRest::Utilities::u2s(response.headers().content_type()) + ")");
							}
							else if(response.status_code() == 429 && response.reason_phrase() == U("Too Many Requests"))
							{
								requestRateLimitTime *= 1.75;
								std::this_thread::sleep_for(std::chrono::seconds(15));
								throw web::http::http_exception(CppRest::Utilities::u2s(response.reason_phrase()));
							}
							else
								throw std::runtime_error("error: unexpected status code (" + std::to_string(response.status_code()) + ") " + CppRest::Utilities::u2s(response.reason_phrase()));
						}).then([=](web::json::value res_json) -> web::json::value
						{
							if(outputDebugFile)
								writeQueryDebugOutputFile(request, authenticated, params, res_json);

							if(res_json.is_null())
								throw std::runtime_error("res_json is null");

							if(res_json.has_field(U("error")))
							{
								utility::string_t errStr = res_json[U("error")].as_string();
								//{ error:"Nonce must be greater than 1460846370855. You provided 2." }
								if(errStr.compare(0, utility::string_t(U("Nonce must be greater than ")).size(), U("Nonce must be greater than ")) == 0)
								{
									errStr = errStr.substr(utility::string_t(U("Nonce must be greater than ")).size());
									nonce_ = stoull(errStr.substr(0, errStr.find(U('.'))));
									throw web::http::http_exception(CppRest::Utilities::u2s(res_json[U("error")].as_string()));
								}
								else if(errStr.compare(0, utility::string_t(U("Error canceling loan order")).size(), U("Error canceling loan order")) != 0)
									throw std::runtime_error("unexpected result: " + CppRest::Utilities::u2s(res_json.serialize()));
							}

							return res_json;
						}).get();

						return ret;
					}
					catch(const web::http::http_exception &e)
					{
						std::cout << "http request exception: " << e.what() << std::endl;
						retry = true;
						std::this_thread::sleep_for(std::chrono::seconds(15));
					}
				}

				return web::json::value();
			}
		};
	}
}
