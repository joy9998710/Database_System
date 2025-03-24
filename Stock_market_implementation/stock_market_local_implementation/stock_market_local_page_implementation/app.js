const express = require('express');
const app = express()
const session = require('express-session');
const homeRouter = require('./routes/home');
const loginRouter = require('./routes/login');
const signupRouter = require('./routes/signup');
const menuRouter = require('./routes/menu');
const userInfoRouter = require('./routes/userInfo');
const depositRouter = require('./routes/deposit');
const withdrawRouter = require('./routes/withdraw');
const stockInfoRouter = require('./routes/stockInfo');
const signoutRouter = require('./routes/signout');
const buyRouter = require('./routes/buy');
const sellRouter = require('./routes/sell');
const holdingStockRouter = require('./routes/holdingStock');
const userTradingLogRouter = require('./routes/userTradingLog');
const stockTradingLogRouter = require('./routes/stockTradingLog');
const orderBookRouter = require('./routes/orderBook');
const tradingVolumeRouter = require('./routes/tradingVolume');
const db = require('./db');

app.use(express.json());
app.use(express.urlencoded({extended : true}));
app.use(session({
    secret: 'answnsdud55!'
}))

app.set('view engine', 'ejs');
app.set('views', './views');

app.use('/', homeRouter);
app.use('/login', loginRouter);
app.use('/signup', signupRouter);
app.use('/menu', menuRouter);
app.use('/userInfo', userInfoRouter);
app.use('/deposit', depositRouter);
app.use('/withdraw', withdrawRouter);
app.use('/stockInfo', stockInfoRouter);
app.use('/signout', signoutRouter);
app.use('/buy', buyRouter);
app.use('/sell', sellRouter);
app.use('/holdingStock', holdingStockRouter);
app.use('/userTradingLog', userTradingLogRouter);
app.use('/stockTradingLog', stockTradingLogRouter);
app.use('/orderBook', orderBookRouter);
app.use('/tradingVolume', tradingVolumeRouter);

app.listen(3000, () => {
    console.log("서버 응답 대기중");
});
