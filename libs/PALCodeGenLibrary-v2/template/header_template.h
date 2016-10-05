public:
	virtual void on_init(void);
	virtual void on_next(void);
	virtual void on_transaction(const tsa::transaction&);

	int *getPositionSeries();
	double *getDailyReturnSeries();
	size_t getNumTradingOpportunities();

private:
	double mProtectiveStopMultiplier;
	double mProfitTargetMultiplier;
	tsa::contract  c;       //tradeable 

	std::vector<int>    m_pos_arr;        //array containing position
	std::vector<double> m_daily_returns;  //array containing returns
	
	double m_exit_price;
	bool m_trade_entry_on_current_bar;
	bool m_long_exit_on_current_bar;
	bool m_short_exit_on_current_bar;

	tsa::order m_entry_order;
	tsa::order m_exit_stop_order;
	tsa::order m_exit_limit_order;
        std::string mSymbolName;
        double mBigPointValue;
        double mTickValue;
        std::string mTableName;

public:
	int signal_when_pattern_matches(
		const tsa::series<double>& open,
		const tsa::series<double>& high,
		const tsa::series<double>& low,
		const tsa::series<double>& close
		);
