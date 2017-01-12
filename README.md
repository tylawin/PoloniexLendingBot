# Poloniex Lending Bot
```
Automatically manage optimizing lend offers for Poloniex API.

Inspired by: https://github.com/Mikadily/poloniexlendingbot
```

# Tested With
```
- Raspbian / c++4.9.2 / Boost 1.55
- Windows / VisualStudio2015 / Boost 1.59
```

# Build
### Requires
- cmake 3.0.1 or greater
- Boost 1.55 or greater
```
git clone https://github.com/tylawin/PoloLendingBot
cd PoloLendingBot
mkdir build
cd build
```
###### Linux: (Raspbian)
```
cmake -DBOOST_LIBRARYDIR="/usr/lib/arm-linux-gnueabihf" ..
```
###### Windows: (MSVC 2015)
```
//32 bit
cmake -DBOOST_ROOT="dir" -DBOOST_LIBRARYDIR="dir" -G "Visual Studio 14 2015" ..
//64 bit
cmake -DBOOST_ROOT="dir" -DBOOST_LIBRARYDIR="dir" -G "Visual Studio 14 2015 Win64" ..
```
###### Linux: (Raspbian)
```
make
source/PoloLendingBot
{editor} config.json // insert your api key and secret
source/PoloLendingBot
```
###### Windows: (MSVC 2015)
```
open PoloniexLendingBot.sln with MSVC
build PoloLendingBot
run PoloLendingBot once
{editor} config.json // insert your api key and secret
run PoloLendingBot
```

# Config Settings
###### global settings (Intervals in seconds)
- apiKey
 - API key string from poloniex. Disable withdraw and trade permissions when setting up API keys in Poloniex for security.
 - Default: "" // Required
- apiSecret
 - API secret string from poloniex.
 - Default: "" // Required
- startupStatisticsInitializeInterval
 - Seconds to wait after startup before creating loan offers to initialize loan rate stats.
 - Loan statistics are currently calculated from 15 minutes worth of fifo queue.
 - Default: 60*15
- updateRateStatisticsInterval
 - Seconds between each rate sample.
 - Default: 10
- refreshLoansInterval
 - Seconds between adjusting loan offer rates and spread amounts. At each interval it cancels all offers and then creates new offers based on current state of statistics, available lending balance, most recent settings from config file, and snapshot of other avaiable offers.
 - Default: 60

###### Per Coin:
- lowestOffersDustSkipAmount
 - Amount of offers to skip before placing the first spread offer.
 - Default: 5
- spreadDustSkipAmount
 - Amount of offers to skip before placing a spread offer.
 - Default: 5
- minRateSkipAmount
 - Minimum rate increment for each spread offer.
 - Default: 0.000001
- lendOrdersToSpread
 - Amount of lend offers to try to spread available lending balance over.
  - May be less since Poloniex requires lend amount >= 0.001
 - Default: 6
- minLendOfferAmount
 - Minimum amount per lend offer. (TODO: automate detection of polo min limit, make this optional)
 - Default: .001
- minTotalLendOrdersToSpread
 - Minimum lend orders to spread over. (Restricts amount of each offer)
 - Default: 30
- maxTotalLendOrdersToSpread
 - Maximum active lend orders. (Restricts spread count)
 - Default: 600
- minDailyRate
 - The minimum rate to lend at.
 - Default: 0.000030
- maxDailyRate
 - The maximum rate to lend at.
 - Default: 0.02
- dayThreshold
 - Days to lend when rate is above setting
 - Default:
```
({ //(APY = (1+(DailyRate*.85)*Days)^(365/Days)-1) // .85 to adjust for polo 15% fee
    { ".0007", 3  },//24% APY 
    { ".0009", 4  },//32% APY 
    { ".0011", 5  },//41% APY
    { ".0015", 7  },//59% APY
    { ".003",  15 },//149% APY
    { ".0045", 30 },//275% APY
    { ".006",  60 }//407% APY
})
```
- autoRenewWhenNotRunning
 - When shutdown cleanly will enable autoRenew for all active loans. (^c once) (^c twice kills)
 - default: true
- maxLendingAccountAmount (TODO - not implemented)
 - Amount exceeding setting will be moved to exchange account. (api keys permission required?)
 - Optional
 - Default: boost::none
- stopLending
 - Stop creating new lend offers. Leave unlent amount in lending account.
 - Default: false

# License
```
Apache License 2.0
See LICENSE file for details.
```

# Future feature notes
- maxLendingAccountAmount: Does api require trade or withdraw permission to move balance from lending to exchange account?
