#ifndef WINDOWS_H_
#define WINDOWS_H_

#include <iostream>
#include <vector>

#include "imgui.h"
#include "implot.h"

#include "../../src/exchange_eventbus.hxx"
#include "../../src/exchanges/coinbase.hxx"

using namespace std;

struct TradeHistory {
  vector<double> time;
  vector<double> price;
  vector<double> volume;
  vector<double> avgs;
  int sum;
};

struct PlotWindow {
  TradeHistory th;
  PlotWindow() {
    auto unsubscribe = EC::ExchangeEventBus::getInstance()->subscribe("COINBASE", "BTC-USD", MD::Channel::TICKER, [this](MD::EventPtr e) {
      MD::TradeEventPtr p = std::dynamic_pointer_cast<MD::TradeEvent>(e);
      cout << "got new price - " << p->t.price << ", "  << p->t.volume << std::endl;
      th.time.push_back(p->t.time/1000000);
      th.price.push_back(p->t.price);
      th.volume.push_back(p->t.volume);
      th.sum += p->t.price;
      th.avgs.push_back(th.sum/th.price.size());
    });

    ImPlot::GetStyle().AntiAliasedLines = true;
  }

  void RenderWindows() {
    ImGui::Begin("CoinBase BTCUSD");
    static ImPlotSubplotFlags flags = ImPlotSubplotFlags_LinkRows;
    if (ImPlot::BeginSubplots("##AxisLinking", 2, 1, ImVec2(-1,-1), flags)) {
      if (ImPlot::BeginPlot("", "", "Price", ImVec2(-1,-1), ImPlotFlags_AntiAliased | ImPlotFlags_Crosshairs, ImPlotAxisFlags_Time | ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit)) {
        if (th.price.size() > 0) {
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Asterisk);
          ImPlot::PlotLine("BTC-USD", &th.time[0], &th.price[0], th.price.size());
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Diamond);
          ImPlot::PlotLine("AVG(BTC-USD)", &th.time[0], &th.avgs[0], th.price.size());
        }
        ImPlot::EndPlot();
      }
      if (ImPlot::BeginPlot("", "Time", "Volume", ImVec2(-1,-1), ImPlotFlags_AntiAliased | ImPlotFlags_Crosshairs, ImPlotAxisFlags_Time | ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit)) {
        if (th.volume.size() > 0) {
          ImPlot::PlotBars("BTC-USD", &th.time[0], &th.volume[0], th.volume.size(), 0.5);
        }
        ImPlot::EndPlot();
      }
      ImPlot::EndSubplots();
    }
    ImGui::End();
  }
};



#endif // WINDOWS_H_
