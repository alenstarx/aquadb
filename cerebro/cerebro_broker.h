#pragma once
#include <list>
#include <memory>
#include <functional>
#include <chrono>
#include "util/common.h"
#include "util/logdef.h"
#include "cerebro_struct.h"
#include "snowflake.h"
#include "nlohmann/json.hpp"

class CerebroAccountWrap
{
  public:
  CerebroAccountWrap(int64_t id, const std::string& name): _aid(id), _name(name) {}

  int64_t id() const { return _aid;}
  const  std::string& name() const { return _name;}

    nlohmann::json to_json() const {
      nlohmann::json js;
      js["id"] = _aid;
      js["name"] = _name;
      // js["cash"] = _cash;
      js["commission_rate"] = _commission_rate;
      js["slippage"] = _slippage;
      js["info"] = _account.to_json();
      return js;
    }
    std::string to_json_string() const {
      auto js = to_json();
      return js.dump();
    }


    // 初始化资金
    inline void set_cash(double cash) { _account.cash = cash; }
    inline double get_cash() const { return _account.cash; }
    inline double total_cash() const { return _account.cash + _account.frozen_cash; }

    inline void set_slippage(double slippage) { _slippage = slippage; }
    inline double get_slippage() const { return _slippage; }

    void set_commission_rate(double commission_rate) { _commission_rate = commission_rate; }
    double get_commission_rate() const { return _commission_rate; }

    double calc_commission(const CerebroOrder &order) const
    {
        return (order.filled_price * order.filled_quantity) * _commission_rate;
    }

    double calc_frozen_cash(const CerebroOrder& order, double last_price = 0.0)
    {
        // 计算需冻结的资金
        return (order.price + _slippage) * order.quantity;
    }

    // 解除冻结, 成交后解除冻结（冻结资金-成交金额）
    double unfreeze_cash(double cash)
    {
        if(cash > _account.frozen_cash)
        {
            _account.frozen_cash = 0.0;
            return 0.0;
        }
        _account.frozen_cash -= cash;
        // TODO
        // 增加可用资金, 实际上冻结的资金用于交易了，解冻的只是之前多冻结的部分资金，此时只需解冻多余的就行，不用增加可用了
        // _account.cash += cash; // 增加可用
        return _account.frozen_cash;
    }

    // 冻结资金, 收到订单后先冻结资金
    void add_frozen_cash(double cash)
    {
        // 冻结部分资金
        _account.frozen_cash += cash;
        // 可用资金减少
        _account.cash -= cash;
    }


    // 分红时使用此接口
    /**
     * @brief 
     * 
     * @param cash 交易资金(不包含费用)
     * @param fee 交易费用
     */
    void add_cash(double cash, double fee = 0.0) {
        _account.cash = _account.cash + cash - fee;
        _account.transaction_cost += fee;
    }


    // 买入时使用
    /**
     * @brief 
     * 
     * @param cash 交易资金(不包含费用)
     * @param fee  交易费用
     */
    int cost_cash(double cash, double fee)
    {
        if((_account.frozen_cash + _account.cash) < (cash + fee))
        {
            // 资金不足
            return -1;
        }

        // 先消费冻结的资金
        _account.frozen_cash = _account.frozen_cash - cash -fee;

        // 冻结资金不够，再消费可用资金
        if(_account.frozen_cash < 0)
        {
            _account.cash += _account.frozen_cash;
            _account.frozen_cash = 0.0;
        }

        // 增加交易费用
        _account.transaction_cost += fee;
    }

    // 入金
    int deposit_cash(double cash, double fee = 0.0)
    {
      _account.cash = _account.cash + cash - fee;
      _account.transaction_cost += fee;

      // 初次入金, 初始化份额和单位价值
      if(util::almost_eq(_account.unit_value, 0.0) && util::almost_eq(_account.units, 0.0))
      {
        _account.unit_value = 1.0;
        _account.units = _account.cash;
      }
      else 
      {
      // 重新计算份额units
         auto units = (_account.unit_value * _account.units + cash)/_account.units;
         _account.units = units;
      }
      return 0;
    }
    // 出金
    int withdraw_cash(double cash)
    {
      if(util::almost_lt(_account.cash, cash))
      {
        // 资金不足
        return -1;
      }
      _account.cash -= cash;

      // 重新计算units;
      auto units = (_account.unit_value * _account.units - cash)/_account.units;
      _account.units = units;
      return 0;
    }

  void update_daily_pnl(double pnl)
  {
    _account.daily_pnl = pnl;
  }

  double market_value() const { return _account.market_value;}

  // 更新市值
  void update_market_value(double value)
  {
    _account.market_value = value;
    // 份额不变， 更新单位价值
    _account.unit_value = (_account.total_cash() + _account.market_value ) / _account.units;
  }

  void update_trading_pnl(double pnl)
  {
    // 份额不变， 更新单位价值
    _account.daily_pnl = pnl;
  }

    // 转市场到资金(卖出)
  int transform_market_value_to_cash(double value, double fee) {
        _account.cash = _account.cash + value - fee;
        _account.transaction_cost += fee;
      

    _account.market_value -= value;
    // 份额不变， 更新单位价值
    _account.unit_value = (_account.total_cash() + _account.market_value ) / _account.units;
    return 0;
  }
    // 转资金到市值(买入)
  int transform_cash_to_market_value(double cash, double fee) {
        _account.cash = _account.cash - cash - fee;
        _account.transaction_cost += fee;
      

    _account.market_value += cash;
    // 份额不变， 更新单位价值
    _account.unit_value = (_account.total_cash() + _account.market_value ) / _account.units;
    return 0;
  }

  inline bool is_initial() const {
    // 市值 和 手续费 同时为0则认为是初始化状态
    if(util::almost_eq(_account.market_value,0.0) && util::almost_eq(_account.transaction_cost, 0.0)) {
      return true;
    }
    return false;
  }
  inline const CerebroAccount& account() const {return _account;}

  private:
  int64_t _aid {0};// 账户ID 
  std::string _name; // 账户名 
    //  double _cash{0};                 // 资金
    double _slippage{0};             // 滑点
    double _commission_rate{0.0003}; // 手续费
    CerebroAccount _account;

};
typedef std::shared_ptr<CerebroAccountWrap> CerebroAccountWrapPtr;

// 撮合器， 一个交易所一个撮合器， 内部再按品种分不同处理模块
class CerebroMatcher
{
  public:
    CerebroMatcher(CerebroBroker *broker);
    virtual ~CerebroMatcher();
    virtual int match(CerebroOrder &order, const CerebroTickRecord &tick) = 0;
    // int do_order_update(const CerebroOrder& order);
    virtual int market_tplus(const Symbol& symbol) { return 0;}
  protected:
    int process_limit_order(CerebroOrder &order, const CerebroTickRecord &tick);
    int process_market_order(CerebroOrder &order, const CerebroTickRecord &tick);
    int process_order_amend(CerebroOrder &order, const CerebroTickRecord &tick);
    int process_order_cancel(CerebroOrder &order, const CerebroTickRecord &tick);

  private:
    CerebroBroker *_broker;
};

// A股撮合器
class CerebroSSEMatcher : public CerebroMatcher
{
  public:
    CerebroSSEMatcher(CerebroBroker *broker);
    virtual ~CerebroSSEMatcher();
    virtual int match(CerebroOrder &order, const CerebroTickRecord &tick) override;
    virtual int market_tplus(const Symbol& symbol) override { return 1;}

  private:
};

// 期货撮合器
class CerebroSZSEMatcher : public CerebroMatcher
{
  public:
    CerebroSZSEMatcher(CerebroBroker *broker);
    virtual ~CerebroSZSEMatcher();
    virtual int match(CerebroOrder &order, const CerebroTickRecord &tick) override;
    virtual int market_tplus(const Symbol& symbol) override { return 1;}

  private:
};

class CerebroPositionTracker
{
  public:
    ~CerebroPositionTracker() {}
    CerebroPositionTracker(const Symbol &symbol, POSITION_DIRECTION direction, int market_tplus = 0);
    // void update(double quantity,double price, double commission);
    void buy(double quantity, double price, double commission = 0.0);
    void sell(double quantity, double price, double commission = 0.0);

    void buy_to_cover(double quantity, double price, double commission = 0.0);
    void sell_short(double quantity, double price, double commission = 0.0);

    const CerebroPosition& current_position() { return _position;}
    inline double current_market_value() const { return _position.market_value() + _position.position_pnl;}
    inline double market_value() const { return _position.market_value();}
    inline double current_position_pnl() const { return _position.position_pnl;}
    inline double current_closable() const { return _position.closable;}
    void update(const CerebroTickRecord& record);
    // 结算
    // 更新持仓收益，可用等。返回结算后的持仓
    CerebroPosition settle(const CerebroTickRecord& record) { return _position;}
  private:
    CerebroPosition _position; //多 or 空
    int _tplus = 0;
};

// 一个用户一个Broker, 用户下可以有多个资金账户
class CerebroBroker
{
  public:
    CerebroBroker() {}
    virtual ~CerebroBroker() {}

    int init(const CerebroConfig& conf);
    int init(CerebroAccountWrapPtr account);
    void set_order_id_generator(std::function<int64_t()> fn) 
    {
      _order_id_gen = fn;
    }
    void set_order_update_callback(std::function<void(const CerebroOrder&)> fn) 
    {
      _order_update_callback = fn;
    }

    int64_t now_ms()
    {
      // TODO, 模拟盘和实盘使用系统时间， 会测使用行情时间
      return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();;
    }

    int now_date() { return -1;}
    // 设置当前交易日(回测时用)
    void set_date() { }

    // 添加资金, 卖出，入金，分红 使用此接口
    /**
     * @brief 
     * 
     * @param cash 交易资金(不包含费用)
     * @param fee 交易费用
     */
    inline void add_cash(double cash, double fee = 0.0) {
        _account->add_cash(cash, fee);
    }

    // 初始化资金
    //inline void set_cash(double cash) { _account->set_cash(cash); }

    inline double get_cash() const { return _account->get_cash(); }
    inline double get_slippage() const { return _account->get_slippage(); }
    inline double get_commission_rate() const { return _account->get_commission_rate(); }



    // 解除冻结, 成交后解除冻结（冻结资金-成交金额）
    inline double unfreeze_cash(double cash)
    {
        return _account->unfreeze_cash(cash);
    }

    // 冻结资金, 收到订单后先冻结资金
    inline void add_frozen_cash(double cash)
    {
      _account->add_frozen_cash(cash);
    }


    // 减少资金, 买入
    /**
     * @brief 
     * 
     * @param cash 交易资金(不包含费用)
     * @param fee  交易费用
     */
    inline int cost_cash(double cash, double fee)
    {
      return  _account->cost_cash(cash, fee);
    }

    double calc_commission(const CerebroOrder &order) const
    {
        return (order.filled_price * order.filled_quantity) * _account->get_commission_rate();
    }

    double calc_frozen_cash(const CerebroOrder& order, double last_price = 0.0)
    {
        // 计算需冻结的资金
        return (order.price + _account->get_slippage()) * order.quantity;
    }


    void set_matcher(const std::string& exchg, CerebroMatcher* matcher)
    {
      auto it = _exchg2matcher.find(exchg);
      if(it != _exchg2matcher.end())
      {
        delete it->second;
      }
      _exchg2matcher[exchg] = matcher;
    }

    void on_tick(const CerebroTickRecord &record); // 接收tick时间，触发订单撮合

    void on_order_update(const CerebroOrder &order);

    // 交易接口，必须返回订单号
    int64_t buy(const Symbol &symbol, double quantity, double price = 0.0);
    int64_t sell(const Symbol &symbol, double quantity, double price = 0.0);

    int64_t buy_to_cover(const Symbol &symbol, double quantity, double price = 0.0);
    int64_t sell_short(const Symbol &symbol, double quantity, double price = 0.0);

    // 撤单视作特殊的订单, 待撤订单id填写在扩展字段中
    int64_t cancel_order(int64_t order_id);

    // 提交订单到队列
    void commit_order(const CerebroOrder &order);

    // 入金, isfinish = true 用于初始化时初次入金
    int64_t deposit_cash(double cash, bool isfinish = false);
    // 出金
    int64_t withdraw_cash(double cash);

    // 结算
    int settle();

    int current_orders(std::vector<CerebroOrder>& orders);
    int current_positions(std::vector<CerebroPosition>& positions);
    double positions_market_value();
    double current_positions_pnl();
    inline CerebroAccountWrapPtr current_account() {return _account;}

    CerebroPositionTracker *get_long_tracker(const Symbol &symbol)
    {
        auto it = _lpositions.find(symbol);
        if (it != _lpositions.cend())
        {
            return it->second;
        }
        auto p = new CerebroPositionTracker(symbol, POSITION_DIRECTION::LONG);
        _lpositions.insert(std::make_pair(symbol, p));
        return p;
    }
    CerebroPositionTracker *get_short_tracker(const Symbol &symbol)
    {
        auto it = _spositions.find(symbol);
        if (it != _spositions.cend())
        {
            return it->second;
        }
        auto p = new CerebroPositionTracker(symbol, POSITION_DIRECTION::SHORT);
        _spositions.insert(std::make_pair(symbol, p));
        return p;
    }
    std::vector<CerebroPositionTracker*> get_all_tracker()
    {
        std::vector<CerebroPositionTracker*> trackers;
        for(auto & item: _lpositions)
        {
          trackers.push_back(item.second);
        }
        for(auto & item: _spositions)
        {
          trackers.push_back(item.second);
        }
        return trackers;
    }

  protected:
    // 处理特殊订单(订单提交时立即处理)
    int process_special_order(const CerebroOrder &order);
    // 处理普通订单(由行情驱动)
    void process_normal_order(const CerebroTickRecord &record);
    // 更新持仓（盈亏）
    void do_position_update(const CerebroTickRecord &record);

  inline bool has_position(const Symbol& symbol) const {
      auto lp = _lpositions.find(symbol);
      if(lp != _lpositions.cend())
      {
        return true;
      }
      auto sp = _spositions.find(symbol);
      if(sp != _spositions.cend())
      {
        return true;
      }
      return false;
  }

    int do_order_amend(CerebroOrder &order);
    int do_order_cancel(CerebroOrder &order);

  private:
    double _cash{0};                 // 资金
    double _slippage{0};             // 滑点
    double _commission_rate{0.0003}; // 手续费
    CerebroAccountWrapPtr _account{nullptr};
    //std::list<CerebroOrder> _unfill_orders; // 未成订单 
    std::map<Symbol,std::list<CerebroOrder>> _unfill_orders; // 未成订单 
    std::vector<CerebroOrder> _filled_orders; // 成交订单
    std::map<Symbol, CerebroPositionTracker *> _lpositions; // 多仓
    std::map<Symbol, CerebroPositionTracker *> _spositions; // 空仓
    std::map<std::string, CerebroMatcher *> _exchg2matcher; // 交易所撮合器 
    std::function<int64_t()> _order_id_gen; // 订单id生成函数
    std::function<void(const CerebroOrder&)> _order_update_callback; // 订单更新回调
};



