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
```
git clone https://github.com/tylawin/PoloLendingBot
```
###### Linux:
```
cd PoloLendingBot
mkdir build
cd build
cmake ..
make
```
###### Run:
```
source/PoloLendingBot
{editor} config.json // insert your api key and secret
source/PoloLendingBot
```

# Config Settings
###### global settings (Intervals in seconds)
- apiKey
- apiSecret
- startupStatisticsInitializeInterval
- updateRateStatisticsInterval
- refreshLoansInterval

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
