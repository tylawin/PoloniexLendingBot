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

#include "PoloniexApi.hpp"
#include "logging.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#undef BOOST_NO_EXCEPTIONS
#include <boost/exception/diagnostic_information.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#ifdef _WIN32
#include <filesystem>
namespace filesystem = std::experimental::filesystem;
#else
#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;
#include <sys/stat.h>
#include <time.h>
#endif

#include <deque>
#include <functional>
#include <map>
#include <unordered_map>

#define Decimal DataTypes::Decimal

namespace tylawin
{
	namespace poloniex
	{
		class PoloniexLendingBot
		{
			struct LentItemInfo
			{
				Amount amount_;
				Rate rate_;
				Amount fees_;
			};

			struct LentAndLendableCurrencyInfo
			{
				Amount amount_;
				Rate rate_;
			};

			class Settings
			{
			public:
				class Coin
				{
				public:
					Amount lowestOffersDustSkipAmount_, spreadDustSkipAmount_;
					Rate minRateSkipAmount_;
					uint32_t minTotalLendOrdersToSpread_, maxTotalLendOrdersToSpread_, lendOrdersToSpread_;
					Rate minDailyRate_, maxDailyRate_;
					std::map<Rate, uint16_t> dayThreshold_;
					bool autoRenewWhenNotRunning_;
					boost::optional<Amount> maxLendingAccountAmount_;//TODO: auto move excess coin from lending to exchange account
					bool stopLending_;//Leave coin in lending account but don't submit new lend orders

					//defaults
					Coin() :
						lowestOffersDustSkipAmount_("5"),
						spreadDustSkipAmount_("5"),
						minRateSkipAmount_(".000001"),
						lendOrdersToSpread_(6),
						minTotalLendOrdersToSpread_(30),
						maxTotalLendOrdersToSpread_(600),
						minDailyRate_(".000030"),//0.9% APY   //".0001" 3.2% APY   //".0003" 10% APY
						maxDailyRate_(".02"),//60days--- 7,104% APY
						dayThreshold_({
								{ ".0007", 3  },//24% APY (APY = (1+(DailyRate*.85)*Days)^(365/Days)-1) // .85 to adjust for polo 15% fee
								{ ".0009", 4  },//32% APY 
								{ ".0011", 5  },//41% APY
								{ ".0015", 7  },//59% APY
								{ ".003",  15 },//149% APY
								{ ".0045", 30 },//275% APY
								{ ".006",  60 }//407% APY
					}),
						autoRenewWhenNotRunning_(true),
						maxLendingAccountAmount_(boost::none),
						stopLending_(false)
					{}

					void ptree(boost::property_tree::ptree &pt)
					{
						lowestOffersDustSkipAmount_ = Amount(pt.get<std::string>("lowestOffersDustSkipAmount"));
						spreadDustSkipAmount_ = Amount(pt.get<std::string>("spreadDustSkipAmount"));

						minRateSkipAmount_ = Amount(pt.get<std::string>("minRateSkipAmount"));
						if(minRateSkipAmount_ < Decimal(".000001") || minRateSkipAmount_ > Decimal(".01"))
							throw std::invalid_argument("minRateSkipAmount(" + to_string(minRateSkipAmount_) + ") valid range is [0.000001, 0.01]");

						lendOrdersToSpread_ = pt.get<int>("lendOrdersToSpread");
						if(lendOrdersToSpread_ < 1 || lendOrdersToSpread_ > 50)
							throw std::invalid_argument("lendOrdersToSpread(" + std::to_string(lendOrdersToSpread_) + ") valid range is [1, 50]");

						minTotalLendOrdersToSpread_ = pt.get<int>("minTotalLendOrdersToSpread");
						if(minTotalLendOrdersToSpread_ < 1 || minTotalLendOrdersToSpread_ > 50000)
							throw std::invalid_argument("minTotalLendOrdersToSpread(" + std::to_string(minTotalLendOrdersToSpread_) + ") valid range is [1, 50000]");

						maxTotalLendOrdersToSpread_ = pt.get<int>("maxTotalLendOrdersToSpread");
						if(maxTotalLendOrdersToSpread_ < 1 || maxTotalLendOrdersToSpread_ > 50000)
							throw std::invalid_argument("maxTotalLendOrdersToSpread(" + std::to_string(maxTotalLendOrdersToSpread_) + ") valid range is [1, 50000]");

						minDailyRate_ = Rate(pt.get<std::string>("minDailyRate"));
						if(minDailyRate_ < Decimal("0.00003") || minDailyRate_ > Decimal("0.05")) // 0.003% daily is 1 % yearly
							throw std::invalid_argument("minDailyRate(" + to_string(minDailyRate_) + ") valid range is [0.00003, 0.05]");

						maxDailyRate_ = Rate(pt.get<std::string>("maxDailyRate"));
						if(maxDailyRate_ < Decimal("0.00003") || maxDailyRate_ > Decimal("0.05"))
							throw std::invalid_argument("maxDailyRate(" + to_string(minDailyRate_) + ") valid range is [0.00003, 0.05]");

						boost::property_tree::ptree pt2 = pt.get_child("rateDayThresholds");
						for(auto pr : pt2)
						{
							boost::property_tree::ptree &pt3 = pr.second;
							Rate tmpRate = Rate(pt3.get<std::string>("ratePercent")) / 100;
							uint16_t tmpDays = pt3.get<uint16_t>("days");

							dayThreshold_[tmpRate] = tmpDays;
						}

						autoRenewWhenNotRunning_ = pt.get<bool>("autoRenewWhenNotRunning");
						maxLendingAccountAmount_ = pt.get_optional<std::string>("maxLendingAccountAmount");
						stopLending_ = pt.get<bool>("stopLending");
					}

					boost::property_tree::ptree ptree()
					{
						boost::property_tree::ptree pt;

						pt.add("lowestOffersDustSkipAmount", lowestOffersDustSkipAmount_);
						pt.add("spreadDustSkipAmount", spreadDustSkipAmount_);
						pt.add("minRateSkipAmount", minRateSkipAmount_);
						pt.add("lendOrdersToSpread", lendOrdersToSpread_);
						pt.add("minTotalLendOrdersToSpread", minTotalLendOrdersToSpread_);
						pt.add("maxTotalLendOrdersToSpread", maxTotalLendOrdersToSpread_);
						pt.add("minDailyRate", minDailyRate_);
						pt.add("maxDailyRate", maxDailyRate_);

						boost::property_tree::ptree rateTmp;
						for(auto rate : dayThreshold_)
						{
							boost::property_tree::ptree pt2;
							pt2.add("ratePercent", rate.first * 100);
							pt2.add("days", rate.second);

							rateTmp.push_back(make_pair("", pt2));
						}
						pt.add_child("rateDayThresholds", rateTmp);

						pt.add("autoRenewWhenNotRunning", autoRenewWhenNotRunning_);
						if(maxLendingAccountAmount_)
							pt.add("maxLendingAccountAmount", maxLendingAccountAmount_);
						pt.add("stopLending", stopLending_);

						return pt;
					}
				};

				struct Data
				{
					std::string apiKey_, apiSecret_;
					std::unordered_map<std::string, Coin> coinSettings_;
					std::chrono::seconds startupStatisticsInitializeInterval_;
					std::chrono::seconds updateRateStatisticsInterval_;
					std::chrono::seconds refreshLoansInterval_;
				};
				Data data_;

				Settings(const Settings &rhs)
				{
					data_.apiKey_ = rhs.data_.apiKey_;
					data_.apiSecret_ = rhs.data_.apiSecret_;
					data_.startupStatisticsInitializeInterval_ = rhs.data_.startupStatisticsInitializeInterval_;
					data_.updateRateStatisticsInterval_ = rhs.data_.updateRateStatisticsInterval_;
					data_.refreshLoansInterval_ = rhs.data_.refreshLoansInterval_;
					data_.coinSettings_ = rhs.data_.coinSettings_;
					settingsFile_ = rhs.settingsFile_;
				}

				Settings(const filesystem::path &settingsFile)
					: settingsFile_(settingsFile)
				{
					data_.startupStatisticsInitializeInterval_ = std::chrono::seconds(60 * 1);
					data_.updateRateStatisticsInterval_ = std::chrono::seconds(10);
					data_.refreshLoansInterval_ = std::chrono::seconds(60);
					data_ = readDataFromFile();
				}

				~Settings() { update(); }

				Settings operator=(const Settings &rhs)
				{
					data_.apiKey_ = rhs.data_.apiKey_;
					data_.apiSecret_ = rhs.data_.apiSecret_;
					data_.coinSettings_ = rhs.data_.coinSettings_;
					data_.startupStatisticsInitializeInterval_ = rhs.data_.startupStatisticsInitializeInterval_;
					data_.updateRateStatisticsInterval_ = rhs.data_.updateRateStatisticsInterval_;
					data_.refreshLoansInterval_ = rhs.data_.refreshLoansInterval_;
					settingsFile_ = rhs.settingsFile_;
					return *this;
				}

			private:
				friend PoloniexLendingBot;

				filesystem::path settingsFile_;
				//filesystem::file_time_type settingsFileModifiedTime_;
				boost::posix_time::ptime settingsFileModifiedTime_;
			
				Data readDataFromFile()
				{
					boost::property_tree::ptree pt;
					if(!filesystem::exists(settingsFile_))
					{
						data_.coinSettings_["BTC"];//add a coin to show default settings
						writeDataToFile(data_);
						throw std::invalid_argument("Settings file did not exist. Insert your poloniex api key and secret values in settings file: " + settingsFile_.string());
					}

					Data tmpData;
					try
					{
						struct stat attrib;
						stat(settingsFile_.string().c_str(), &attrib);
						settingsFileModifiedTime_ = boost::posix_time::ptime_from_tm(*gmtime(&(attrib.st_mtime)));
						//settingsFileModifiedTime_ = filesystem::last_write_time(settingsFile_);

						read_json(settingsFile_.string(), pt);

						tmpData.apiKey_ = pt.get<std::string>("key");
						tmpData.apiSecret_ = pt.get<std::string>("secret");
						if(tmpData.apiKey_.size() == 0 || tmpData.apiSecret_.size() == 0)
							throw std::invalid_argument("Insert your poloniex api key and secret values in settings file: " + settingsFile_.string());

						tmpData.startupStatisticsInitializeInterval_ = std::chrono::seconds(pt.get<int>("startupStatisticsInitializeInterval"));
						if(tmpData.startupStatisticsInitializeInterval_ < std::chrono::seconds(1) || tmpData.startupStatisticsInitializeInterval_ > std::chrono::seconds(3600*24))
							throw std::invalid_argument("startupStatisticsInitializeInterval(" + std::to_string(tmpData.startupStatisticsInitializeInterval_.count()) + ") valid range is [1, 3600*24] seconds");

						tmpData.updateRateStatisticsInterval_ = std::chrono::seconds(pt.get<int>("updateRateStatisticsInterval"));
						if(tmpData.updateRateStatisticsInterval_ < std::chrono::seconds(1) || tmpData.updateRateStatisticsInterval_ > std::chrono::seconds(3600))
							throw std::invalid_argument("updateRateStatisticsInterval(" + std::to_string(tmpData.updateRateStatisticsInterval_.count()) + ") valid range is [1, 3600] seconds");

						tmpData.refreshLoansInterval_ = std::chrono::seconds(pt.get<int>("refreshLoansInterval"));
						if(tmpData.refreshLoansInterval_ < std::chrono::seconds(1) || tmpData.refreshLoansInterval_ > std::chrono::seconds(3600))
							throw std::invalid_argument("refreshLoansInterval(" + std::to_string(tmpData.refreshLoansInterval_.count()) + ") valid range is [1, 3600] seconds");

						boost::property_tree::ptree pt2 = pt.get_child("CoinSettings");
						std::string curCode;
						for(auto pr : pt2)
						{
							curCode = pr.first;
							try
							{
								tmpData.coinSettings_[curCode].ptree(pr.second);
							}
							catch(const std::invalid_argument &e)
							{
								throw std::invalid_argument("curCode(" + curCode + ") " + e.what());
							}
						}
					}
					catch(const std::invalid_argument &e)
					{
						throw;
					}
					catch(const boost::exception &e)//TODO: boost::property_tree specific exceptions???
					{
						throw std::invalid_argument("Parse error: " + boost::current_exception_diagnostic_information());
					}
					catch(...)
					{
						throw std::runtime_error("Settings file unhandled load error: " + boost::current_exception_diagnostic_information());
					}

					return tmpData;
				}

				void writeDataToFile(const Data &data)
				{
					boost::property_tree::ptree pt;

					pt.add("key", data.apiKey_);
					pt.add("secret", data.apiSecret_);
					pt.add("startupStatisticsInitializeInterval", data.startupStatisticsInitializeInterval_.count());
					pt.add("updateRateStatisticsInterval", data.updateRateStatisticsInterval_.count());
					pt.add("refreshLoansInterval", data.refreshLoansInterval_.count());

					boost::property_tree::ptree coinPt;
					for(auto coinSetting : data.coinSettings_)
					{
						coinPt.add_child(coinSetting.first, coinSetting.second.ptree());
					}
					pt.add_child("CoinSettings", coinPt);

					write_json(settingsFile_.string(), pt);

					struct stat attrib;
					stat(settingsFile_.string().c_str(), &attrib);
					settingsFileModifiedTime_ = boost::posix_time::ptime_from_tm(*gmtime(&(attrib.st_mtime)));
					//settingsFileModifiedTime_ = filesystem::last_write_time(settingsFile_);
				}

			public:
				//Data in file has priority. To delete coin settings shutdown bot first.
				void update()
				{
					struct stat attrib;
					stat(settingsFile_.string().c_str(), &attrib);
					boost::posix_time::ptime tmpTime = boost::posix_time::ptime_from_tm(*gmtime(&(attrib.st_mtime)));
					if(settingsFileModifiedTime_ != tmpTime)
					//if(settingsFileModifiedTime_ != filesystem::last_write_time(settingsFile_))
					{
						Data tmpData = readDataFromFile();

						data_.apiKey_ = tmpData.apiKey_;
						data_.apiSecret_ = tmpData.apiSecret_;
						data_.startupStatisticsInitializeInterval_ = tmpData.startupStatisticsInitializeInterval_;
						data_.updateRateStatisticsInterval_ = tmpData.updateRateStatisticsInterval_;
						data_.refreshLoansInterval_ = tmpData.refreshLoansInterval_;
						for(auto pr : tmpData.coinSettings_)
						{
							data_.coinSettings_[pr.first] = pr.second;
						}
					}

					writeDataToFile(data_);
				}
			};

			std::map<CurrencyCode, LentItemInfo> totalLent_;
			std::map<CurrencyCode, LentAndLendableCurrencyInfo> totalLentAndLendable_;
			std::unordered_map<CurrencyCode, uint32_t> loanCount_;
			bool dryRun_ = false;
			Settings settings_;
			PoloniexApi poloApi;
			std::function<bool()> doQuit_;
			PoloniexApi::ActiveLoans activeLoans_;
			std::unordered_map<CurrencyCode, uint32_t> curGetLoanOrdersFloatingLimit_;

		public:
			void dryRun(const bool setValue) { dryRun_ = setValue; }

			PoloniexLendingBot(std::function<bool()> doQuit, filesystem::path settingsFile = "config.json") :
				settings_(settingsFile),
				poloApi(settings_.data_.apiKey_, settings_.data_.apiSecret_),
				doQuit_(doQuit)
			{}

			void refreshActiveLoansAndTotalLent()
			{
				auto lendingAccountBalances = poloApi.getAvailableAccountBalances(PoloniexApi::AccountTypes::LENDING)[U("lending")];
				auto loanOffers = poloApi.getOpenLoanOffers();
				activeLoans_ = poloApi.getActiveLoans();

				for(auto& lent : totalLent_)
				{
					lent.second.amount_ = 0;
					lent.second.rate_ = 0;
					lent.second.fees_ = 0;
				}
				for(auto &lentable : totalLentAndLendable_)
				{
					lentable.second.amount_ = 0;
					lentable.second.rate_ = 0;
				}
				
				if(lendingAccountBalances.size() != 0)
				{
					for(auto pairCurrencyBalance : lendingAccountBalances.as_object())
					{
						CurrencyCode curCode = CppRest::Utilities::u2s(pairCurrencyBalance.first);

						totalLentAndLendable_[curCode].amount_ = CppRest::Utilities::u2s(pairCurrencyBalance.second.as_string());
						totalLentAndLendable_[curCode].rate_ = 0;
					}
				}

				if(loanOffers.size() != 0)
				{
					std::unordered_map<std::string, Amount> onOrderBalances;
					for(auto cur : loanOffers.as_object())
					{
						CurrencyCode loanCurCode = CppRest::Utilities::u2s(cur.first);
						for(auto offer : loanOffers[CppRest::Utilities::s2u(loanCurCode)].as_array())
						{
							if(totalLentAndLendable_.find(loanCurCode) != totalLentAndLendable_.end())
							{
								totalLentAndLendable_[loanCurCode].amount_ += CppRest::Utilities::u2s(offer[U("amount")].as_string());
								totalLentAndLendable_[loanCurCode].rate_ += 0;
							}
							else
							{
								totalLentAndLendable_[loanCurCode].amount_ = CppRest::Utilities::u2s(offer[U("amount")].as_string());
								totalLentAndLendable_[loanCurCode].rate_ = 0;
							}
						}
					}
				}

				for(auto pr : activeLoans_)
				{
					CurrencyCode curCode = pr.first;
					auto curActiveLoans = pr.second;
					for(auto item : curActiveLoans)
					{
						auto& loan = item.second;
						if(totalLent_.find(curCode) != totalLent_.end())
						{
							totalLent_[curCode].amount_ += loan.amount_;
							totalLent_[curCode].rate_ += loan.rate_ * loan.amount_;
							totalLent_[curCode].fees_ += loan.fees_;
						}
						else
						{
							totalLent_[curCode].amount_ = loan.amount_;
							totalLent_[curCode].rate_ = loan.rate_ * loan.amount_;
							totalLent_[curCode].fees_ = loan.fees_;
						}

						if(totalLentAndLendable_.find(curCode) != totalLentAndLendable_.end())
						{
							totalLentAndLendable_[curCode].amount_ += loan.amount_;
							totalLentAndLendable_[curCode].rate_ += loan.rate_ * loan.amount_;
						}
						else
						{
							totalLentAndLendable_[curCode].amount_ = loan.amount_;
							totalLentAndLendable_[curCode].rate_ = loan.rate_ * loan.amount_;
						}
					}
				}

				for(auto iter = totalLent_.begin(); iter != totalLent_.end(); )
				{
					if(iter->second.amount_ == 0)
						iter = totalLent_.erase(iter);
					else
						++iter;
				}
				for(auto iter = totalLentAndLendable_.begin(); iter != totalLentAndLendable_.end(); )
				{
					if(iter->second.amount_ == 0)
						iter = totalLentAndLendable_.erase(iter);
					else
						++iter;
				}
			}

			std::string getStatusStringLentAmountAndRates()
			{
				std::string result = "Lent: ";
				CurrencyCode key;
				for(auto iter = totalLent_.begin(); iter != totalLent_.end(); ++iter)
				{
					key = iter->first;
					result += "[" + to_string(totalLent_[key].amount_, 4) + " " + key;
					if(totalLent_[key].amount_ > 0)
						result += " @ " + to_string(totalLent_[key].rate_ * 100 / totalLent_[key].amount_, 4) + "%";
					result += "] ";
				}
				return result;
			}

			std::string getStatusStringTotalLentAndLendAccountAmountsAndRates()
			{
				std::string result = "Total:";
				CurrencyCode key;
				for(auto iter = totalLentAndLendable_.begin(); iter != totalLentAndLendable_.end(); ++iter)
				{
					key = iter->first;
					result += "[" + to_string(totalLentAndLendable_[key].amount_, 4) + " " + key;
					if(totalLentAndLendable_[key].amount_ > 0)
						result += " @ " + to_string(totalLentAndLendable_[key].rate_ * 100 / totalLentAndLendable_[key].amount_, 4) + "%";
					result += "] ";
				}
				return result;
			}

			void createLoanOffer(CurrencyCode curCode, Amount amt, Rate rate)
			{
				if(amt < PoloniexApi::minimumLendAmount_)
					throw std::invalid_argument(__FILE__ ":" STR__LINE__ " - invalid amount. " + to_string(amt) + " not >= " + std::to_string(PoloniexApi::minimumLendAmount_));

				uint16_t days = 2;
				for(auto pr : settings_.data_.coinSettings_.at(curCode).dayThreshold_)
				{
					if(days < pr.second && rate >= pr.first)
						days = pr.second;
				}

				std::string amtStr = to_string(amt);

				web::json::value response;
				if(dryRun_ == false)
					response = poloApi.createLoanOffer(curCode, amtStr, days, 0, to_string(rate, 6));
				else
					response[U("message")] = web::json::value(U("dryrun"));

				INFO << " Created loan offer: " << amtStr << " " << curCode << " at " << to_string(rate * 100, 4) << "% for " << days << "days... " << CppRest::Utilities::u2s(response.serialize());
			}

			//if curCode not supplied then cancel all currencies
			void cancelAllOpenLoanOffers(const boost::optional<const CurrencyCode &> curCode = boost::none)
			{
				if(dryRun_ == true)
					return;
				auto loanOffers = poloApi.getOpenLoanOffers();
				if(loanOffers.size() == 0)//api returns array when empty instead of object... [] vs {} then the next loop crashes...
					return;

				std::unordered_map<std::string, Amount> onOrderBalances;
				for(auto cur : loanOffers.as_object())
				{
					CurrencyCode loanCurCode = CppRest::Utilities::u2s(cur.first);
					if(!curCode || (*curCode == loanCurCode))
					{
						for(auto offer : loanOffers[CppRest::Utilities::s2u(loanCurCode)].as_array())
						{
							onOrderBalances[loanCurCode] += Amount(CppRest::Utilities::u2s(offer[U("amount")].as_string()));
							web::json::value msg;
							msg[U("message")] = web::json::value("dryrun");
							if(dryRun_ == false)
								msg = poloApi.cancelLoanOffer(offer[U("id")].as_integer());
							INFO << " Canceling " << loanCurCode << " order... " << msg.serialize();
						}
					}
				}
			}

			boost::optional<Rate> lowestOfferRateAboveDustAmount(PoloniexApi::LoanOrders::Offers &loanOffers, CurrencyCode curCode)
			{
				Amount amt(0);
				for(auto offer : loanOffers)
				{
					amt += offer.second.amount_;
					if(amt >= settings_.data_.coinSettings_.at(curCode).lowestOffersDustSkipAmount_)
						return offer.first;
				}
				return boost::none;
			}

		private:
			class LendingStatistics
			{
			public:
				class Coin
				{
				public:
					std::deque<Rate> lendingRateHist_15m;
					Decimal lendingRateLow_15m;
					Decimal lendingRateHigh_15m;
					Decimal movingAvgLendingRate_15m;

					Coin() :
						lendingRateLow_15m(-1),
						lendingRateHigh_15m(-1),
						movingAvgLendingRate_15m(-1)
					{}
				};

				static Rate lowestRate(const std::deque<Rate> &dq)
				{
					Rate min(500000);
					for(auto rate : dq)
					{
						if(rate < min)
							min = rate;
					}
					return min;
				}

				static Rate highestRate(const std::deque<Rate> &dq)
				{
					Rate max(0);
					for(auto rate : dq)
					{
						if(rate > max)
							max = rate;
					}
					return max;
				}

				static Rate averageRate(const std::deque<Rate> &dq)
				{
					Rate avgSum(0);
					for(auto rate : dq)
						avgSum += rate;
					return avgSum / dq.size();
				}

				std::unordered_map<CurrencyCode, Coin> coinStats_;
			private:
			};
			LendingStatistics lendingStatistics_;

			boost::optional<uint32_t> calcPositionOfLastOfferToSpreadLendUnder(const CurrencyCode &curCode, PoloniexApi::LoanOrders::Offers &loanOffers)
			{
				uint32_t offerCount = 0;
				uint16_t spreadCount = 0;

				Amount sum;
				for(auto offer : loanOffers)
				{
					sum += offer.second.amount_;
					if(spreadCount == 0 && sum >= settings_.data_.coinSettings_.at(curCode).lowestOffersDustSkipAmount_)
					{
						++spreadCount;
						sum = 0;
					}
					else if(spreadCount != 0 && sum >= settings_.data_.coinSettings_.at(curCode).spreadDustSkipAmount_)
					{
						++spreadCount;
						sum = 0;
					}
					++offerCount;
					if(spreadCount >= settings_.data_.coinSettings_.at(curCode).lendOrdersToSpread_)
						return offerCount;
				}

				return boost::none;
			}

			PoloniexApi::LoanOrders::Offers getLoanOrdersAndAdjustLimit(const CurrencyCode &curCode)
			{
				if(curGetLoanOrdersFloatingLimit_.find(curCode) == curGetLoanOrdersFloatingLimit_.end())
					curGetLoanOrdersFloatingLimit_.insert(std::make_pair(curCode, 100));

				auto loans = poloApi.getLoanOrders(curCode, curGetLoanOrdersFloatingLimit_[curCode]);

				auto lastPos = calcPositionOfLastOfferToSpreadLendUnder(curCode, loans.offers_);

				if(lastPos && (*lastPos) < curGetLoanOrdersFloatingLimit_[curCode] / 2)
				{
					if(curGetLoanOrdersFloatingLimit_[curCode] > 50)
						curGetLoanOrdersFloatingLimit_[curCode] -= 2;
				}
				else
				{
					while(!lastPos && loans.offers_.size() == curGetLoanOrdersFloatingLimit_[curCode])
					{
						if(curGetLoanOrdersFloatingLimit_[curCode] >= 1500)
							break;

						curGetLoanOrdersFloatingLimit_[curCode] += 20;

						loans = poloApi.getLoanOrders(curCode, curGetLoanOrdersFloatingLimit_[curCode]);

						lastPos = calcPositionOfLastOfferToSpreadLendUnder(curCode, loans.offers_);
					}
				}

				return loans.offers_;
			}

		public:
			void lendingRateStatistics()
			{
				std::ostringstream msg;
				for(auto coin : settings_.data_.coinSettings_)
				{
					std::string curCode = coin.first;
					LendingStatistics::Coin &coinStats = lendingStatistics_.coinStats_[curCode];

					//TODO: if(lent + lendable == 0)
					//  delete stats // need logic elsewhere to collect stats before createOffers if lendable added after initial startup
					//	continue;

					auto loans = getLoanOrdersAndAdjustLimit(curCode);

					auto lowestRate = lowestOfferRateAboveDustAmount(loans, curCode);
					if(!lowestRate)
						lowestRate = coin.second.maxDailyRate_;

					coinStats.lendingRateHist_15m.push_front(*lowestRate);
					if(coinStats.lendingRateHist_15m.size() > 6 * 15)
						coinStats.lendingRateHist_15m.pop_back();
					coinStats.lendingRateLow_15m = LendingStatistics::lowestRate(coinStats.lendingRateHist_15m);
					coinStats.lendingRateHigh_15m = LendingStatistics::highestRate(coinStats.lendingRateHist_15m);
					coinStats.movingAvgLendingRate_15m = LendingStatistics::averageRate(coinStats.lendingRateHist_15m);
					msg << "[" << curCode << "(low:" << to_string(coinStats.lendingRateLow_15m * 100, 4) << "% dust:" << to_string((*lowestRate) * 100, 4) << "% avg:" << to_string(coinStats.movingAvgLendingRate_15m * 100, 4) << "% high:" << to_string(coinStats.lendingRateHigh_15m * 100, 4) << "%)] ";
				}
				INFO << msg.str();
			}

			Rate firstLendOfferRate(PoloniexApi::LoanOrders::Offers &availableLoans, const CurrencyCode &curCode, const LendingStatistics::Coin &coinStats)
			{
				const auto& coinSettings = settings_.data_.coinSettings_[curCode];

				boost::optional<Decimal> lowestOfferRateAboveDust = lowestOfferRateAboveDustAmount(availableLoans, curCode);
				Rate beginningRateAboveDust = lowestOfferRateAboveDust ? *lowestOfferRateAboveDust : coinSettings.maxDailyRate_ + PoloniexApi::minimumRateIncrement_;

				if(beginningRateAboveDust < coinSettings.minDailyRate_)
					beginningRateAboveDust = coinSettings.minDailyRate_;

				if(beginningRateAboveDust < (coinStats.lendingRateLow_15m + coinStats.movingAvgLendingRate_15m) / 2)
					beginningRateAboveDust = (coinStats.lendingRateLow_15m + coinStats.movingAvgLendingRate_15m) / 2;

				return beginningRateAboveDust;
			}

			Amount calcSpreadLendAmount(const CurrencyCode &curCode, const Amount &availableLendBalance)
			{
				const auto& coinSettings = settings_.data_.coinSettings_[curCode];

				uint32_t tmpSpreadLendCount = coinSettings.lendOrdersToSpread_;

				uint32_t activeLoanCount = activeLoans_[curCode].size();

				if(activeLoanCount + tmpSpreadLendCount < coinSettings.minTotalLendOrdersToSpread_)
					tmpSpreadLendCount = coinSettings.minTotalLendOrdersToSpread_;

				if(activeLoanCount + tmpSpreadLendCount > coinSettings.maxTotalLendOrdersToSpread_)
					tmpSpreadLendCount = coinSettings.maxTotalLendOrdersToSpread_ - activeLoanCount;

				while(availableLendBalance / tmpSpreadLendCount < PoloniexApi::minimumLendAmount_)
				{
					tmpSpreadLendCount -= 1;
					if(tmpSpreadLendCount == 0)
					{
						WARN << "balance < " << PoloniexApi::minimumLendAmount_;
						return Amount(0);
					}
				}

				return availableLendBalance / tmpSpreadLendCount;
			}

			void createSpreadLendOffers(const CurrencyCode &curCode, Amount availableLendBalance)
			{
				const auto& coinSettings = settings_.data_.coinSettings_[curCode];
				LendingStatistics::Coin &coinStats = lendingStatistics_.coinStats_[curCode];

				auto availableLoans = getLoanOrdersAndAdjustLimit(curCode);

				loanCount_[curCode] = 0;

				Rate beginningRateAboveDust = firstLendOfferRate(availableLoans, curCode, coinStats);

				if(beginningRateAboveDust >= coinSettings.maxDailyRate_ || availableLoans.size() == 0)
				{
					if(beginningRateAboveDust == coinSettings.maxDailyRate_)
						createLoanOffer(curCode, availableLendBalance, coinSettings.maxDailyRate_ - PoloniexApi::minimumRateIncrement_);
					else
						createLoanOffer(curCode, availableLendBalance, coinSettings.maxDailyRate_);
				}
				else
				{
					Amount spreadLendAmount = calcSpreadLendAmount(curCode, availableLendBalance);
					
					spreadLendAmount = Amount(to_string(spreadLendAmount,8));//round off

					uint16_t createLoanOfferCount = 0;
					Amount offerAmountSum(0);
					Rate previousCreatedOfferRate(0);
					for(auto offer : availableLoans)
					{
						const Rate &rate = offer.first;
						if((rate - PoloniexApi::minimumRateIncrement_) - previousCreatedOfferRate < coinSettings.minRateSkipAmount_)
							continue;

						offerAmountSum += offer.second.amount_;

						if(offerAmountSum > coinSettings.spreadDustSkipAmount_ && rate >= beginningRateAboveDust && offer.second.amount_ > coinSettings.spreadDustSkipAmount_ / 2)
						{

							if(availableLendBalance - spreadLendAmount < 0 || availableLendBalance - spreadLendAmount < PoloniexApi::minimumLendAmount_)
								spreadLendAmount = availableLendBalance;
							previousCreatedOfferRate = rate - PoloniexApi::minimumRateIncrement_;
							createLoanOffer(curCode, spreadLendAmount, previousCreatedOfferRate);
							availableLendBalance -= spreadLendAmount;
							++createLoanOfferCount;
							offerAmountSum = 0;
						}
						if(availableLendBalance == 0 || createLoanOfferCount >= coinSettings.lendOrdersToSpread_)
							break;
					}

					if(availableLendBalance > PoloniexApi::minimumLendAmount_ && previousCreatedOfferRate + PoloniexApi::minimumRateIncrement_ < coinStats.lendingRateHigh_15m)
					{
						const Rate &lastRate = availableLoans.rbegin()->first;
						if(lastRate < coinStats.lendingRateHigh_15m)
						{
							if(availableLendBalance - spreadLendAmount != 0.0 && availableLendBalance - spreadLendAmount < PoloniexApi::minimumLendAmount_)
								spreadLendAmount = availableLendBalance;
							createLoanOffer(curCode, spreadLendAmount, coinStats.lendingRateHigh_15m - PoloniexApi::minimumRateIncrement_);
							availableLendBalance -= spreadLendAmount;
						}
					}

					if(availableLendBalance > PoloniexApi::minimumLendAmount_)
						createLoanOffer(curCode, availableLendBalance, coinSettings.maxDailyRate_);
				}
			}

			void refreshLoans()
			{
				cancelAllOpenLoanOffers();

				auto tmpJsonValue = poloApi.getAvailableAccountBalances(PoloniexApi::AccountTypes::LENDING)[U("lending")];
				if(tmpJsonValue.size() == 0)
					return;
				auto lendingBalances = tmpJsonValue.as_object();

				auto tmp = poloApi.getOpenLoanOffers();

				for(auto activeCur : lendingBalances)
				{
					CurrencyCode curCode = CppRest::Utilities::u2s(activeCur.first);

					if(settings_.data_.coinSettings_[curCode].stopLending_)
						continue;
					
					Amount availableBalance(CppRest::Utilities::u2s(activeCur.second.as_string()));

					if(availableBalance >= PoloniexApi::minimumLendAmount_)
						createSpreadLendOffers(curCode, availableBalance);
				}

				refreshActiveLoansAndTotalLent();
			}

			void setAllAutoRenew(bool autoRenew)
			{
				if(dryRun_ == true)
					return;

				size_t i(0);
				try
				{
					std::string action = (autoRenew ? "Enabling" : "Disabling");
					INFO << action << " autoRenew for all active loans...";
					auto cryptoLent = poloApi.getActiveLoans();
					for(auto currencyActiveLent : cryptoLent)
					{
						const CurrencyCode &curCode = currencyActiveLent.first;
						for(auto pr : currencyActiveLent.second)
						{
							PoloniexApi::LoanId loanId = pr.first;
							auto &loan = pr.second;
							if(autoRenew == false && loan.autoRenew_
								|| autoRenew == true && (settings_.data_.coinSettings_.find(curCode) == settings_.data_.coinSettings_.end()
								|| settings_.data_.coinSettings_.at(curCode).autoRenewWhenNotRunning_))
							{
								INFO << "  " << action << " autoRenew for " << curCode << "loan id(" << loanId << ") - worst case progress (count/totalLoans): " + std::to_string(i) + "/" + std::to_string(currencyActiveLent.second.size());
								try
								{
									poloApi.toggleAutoRenew(loanId);
								}
								catch(const std::exception &e)//TODO: limit to certain exceptions?
								{
									WARN << "   Failed. error: " << e.what();
								}
								loan.autoRenew_ = !loan.autoRenew_;
								++i;
							}
						}
					}
				}
				catch(const std::exception &e)
				{
					WARN << "   Failed. error: " << e.what();
					exit(EXIT_FAILURE);
				}
				INFO << (autoRenew ? "Enabled" : "Disabled") << " AutoRenew for " << i << " loans";
			}

			int run()
			{
				//turn off autoRenew on loans since we are running now
				setAllAutoRenew(false);

				//cancel suboptimal open offers
				cancelAllOpenLoanOffers();

				refreshActiveLoansAndTotalLent();
				INFO << getStatusStringLentAmountAndRates();
				INFO << getStatusStringTotalLentAndLendAccountAmountsAndRates();

				boost::posix_time::ptime nowTime;
				boost::posix_time::ptime startTime = boost::posix_time::second_clock::universal_time();
				while(true)//establish moving average before setting our lending rate
				{
					try
					{
						lendingRateStatistics();
						std::this_thread::sleep_for(settings_.data_.updateRateStatisticsInterval_);
						nowTime = boost::posix_time::second_clock::universal_time();
						if(dryRun_)
						{
							INFO << "dryrun -- skipping wait to get statistics";
							break;
						}
						if(nowTime - startTime > boost::posix_time::seconds(settings_.data_.startupStatisticsInitializeInterval_.count()))
							break;
					}
					catch(const std::exception &e)
					{
						ERROR << boost::diagnostic_information(e);
						std::this_thread::sleep_for(std::chrono::seconds(10));
						continue;
					}

					if(doQuit_())
					{
						setAllAutoRenew(true);
						INFO << "^c - quit";
						return 0;
					}
				}

				refreshActiveLoansAndTotalLent();
				startTime = boost::posix_time::second_clock::universal_time() - boost::posix_time::seconds(settings_.data_.refreshLoansInterval_.count());
				while(true)
				{
					try
					{
						nowTime = boost::posix_time::second_clock::universal_time();
						lendingRateStatistics();
						if(nowTime - startTime >= boost::posix_time::seconds(settings_.data_.refreshLoansInterval_.count()))
						{
							try
							{
								settings_.update();
							}
							catch(const std::exception &e)
							{
								WARN << "Reading settings file failed. (" << boost::diagnostic_information(e) << ") Continuing with old settings.";
							}

							startTime = nowTime;

							refreshLoans();

							INFO << getStatusStringLentAmountAndRates();
							INFO << getStatusStringTotalLentAndLendAccountAmountsAndRates();
						}
						std::this_thread::sleep_for(settings_.data_.updateRateStatisticsInterval_);
					}
					catch(const std::exception &e)
					{
						ERROR << boost::diagnostic_information(e);
						std::this_thread::sleep_for(std::chrono::seconds(10));
						continue;
					}

					if(doQuit_())
					{
						setAllAutoRenew(true);
						return 0;
					}
				}

				return 1;
			}
		};
	}
}
