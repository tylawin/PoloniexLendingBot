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

#include <boost/log/expressions.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>
//#include <boost/core/null_deleter.hpp> // too new for 1.55
#include <boost/utility/empty_deleter.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void logInit()
{
	boost::log::add_common_attributes();
	
	typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend> text_sink;
	auto consoleSink = boost::make_shared<text_sink>();
//	consoleSink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
	consoleSink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::empty_deleter()));
	consoleSink->locked_backend()->auto_flush(true);
	boost::log::formatter consoleFormatter = boost::log::expressions::stream << boost::log::expressions::smessage;
	consoleSink->set_formatter(consoleFormatter);
	consoleSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
	
	boost::log::core::get()->add_sink(consoleSink);
	
	auto fileSink = boost::make_shared<text_sink>();
	fileSink->locked_backend()->add_stream(boost::make_shared<std::ofstream>("PoloLendingBot_log.txt", std::ios::out | std::ios::app));
	fileSink->locked_backend()->auto_flush(true);
	boost::log::formatter fileFormatter = boost::log::expressions::stream
		<< boost::log::expressions::format_date_time(timestamp, "[%Y-%m-%d %H:%M:%S.%f] ")
		<< "[" << boost::log::trivial::severity << "]"
		<< " - " << boost::log::expressions::smessage;
	fileSink->set_formatter(fileFormatter);
	fileSink->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
	
	boost::log::core::get()->add_sink(fileSink);

	boost::log::core::get()->set_filter
	(
		boost::log::trivial::severity >= boost::log::trivial::info
	);
}

#define INFO  BOOST_LOG_TRIVIAL(info)
#define WARN  BOOST_LOG_TRIVIAL(warning)
#define ERROR BOOST_LOG_TRIVIAL(error)
