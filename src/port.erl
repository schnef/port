-module(port).

-behaviour(gen_server).

%% API
-export([start_link/0, foo/2, bar/3]).

%% gen_server callbacks
-export([init/1, handle_call/3, handle_cast/2, handle_info/2,
	 terminate/2, code_change/3]).

-define(SERVER, ?MODULE).

-record(state, {port}).

%%%===================================================================
%%% API
%%%===================================================================

foo(A, B) ->
    gen_server:call(?SERVER, {foo, A, B}).

bar(A, B, C) ->
    gen_server:call(?SERVER, {bar, A, B, C}).

start_link() ->
    gen_server:start_link({local, ?SERVER}, ?MODULE, [], []).

%%%===================================================================
%%% gen_server callbacks
%%%===================================================================

init([]) ->
    Port = case code:priv_dir(port) of % argument is the name of the application
	       {error, _} ->
		   exit(error);
	       Priv_dir ->
		   open_port({spawn, filename:join([Priv_dir, "port"])},
			     [binary, {packet, 4}, exit_status])
	   end,
    error_logger:info_msg("Port ~p", [erlang:port_info(Port)]),
    {ok, #state{port = Port}}.

handle_call(Msg, _From, #state{port = Port} = State) ->
    Port ! {self(), {command, encode(Msg)}},
    receive
    	{Port, {data, Data}} ->
    	    {reply, decode(Data), State}
    end;
handle_call(_Request, _From, State) ->
    Reply = ok,
    {reply, Reply, State}.

handle_cast(_Msg, State) ->
    {noreply, State}.

handle_info({'EXIT', Port, Reason}, State) ->
    error_logger:format("port ~p exited with reason ~p", [Port, Reason]),
    {stop, Reason, State};
handle_info({Port, {exit_status, Status}}, #state{port = Port} = State) when Status > 128 ->
    error_logger:error_msg("Port terminated with signal: ~p", [Status - 128]),
    {stop, {port_terminated, Status}, State};
handle_info({Port, {exit_status, Status}}, #state{port = Port} = State) ->
    decode_exit_status(Status),
    {stop, {port_terminated, Status}, State};
handle_info(Info, State) ->
    error_logger:warning_msg("Unhandled info ~p", [Info]),
    {noreply, State}.

terminate(_Reason, _State) ->
    ok.

code_change(_OldVsn, State, _Extra) ->
    {ok, State}.

%%%===================================================================
%%% Internal functions
%%%===================================================================

encode({foo, A, B}) ->
    term_to_binary({1, A, B});
encode({bar, A, B, C}) ->
    term_to_binary({2, A, B, C}).

decode(Data) ->
    binary_to_term(Data).

decode_exit_status(1) -> error_logger:error_msg("Could not allocate memory");
decode_exit_status(2) -> error_logger:error_msg("Could not free memory");
decode_exit_status(3) -> error_logger:error_msg("Could not read from stdin");
decode_exit_status(4) -> error_logger:error_msg("Could not read packet header");
decode_exit_status(5) -> error_logger:error_msg("Could not read packet body");
decode_exit_status(6) -> error_logger:error_msg("Wrong packet body size");
decode_exit_status(7) -> error_logger:error_msg("Could not open library");
decode_exit_status(Status) -> error_logger:error_msg("Port terminated with status: ~p", [Status]).
