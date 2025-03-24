const express = require('express');
const router = express.Router();
const db = require('../db');

router.get('/', (req, res) => {
    db.query('SELECT * FROM user WHERE user_id = ?', [req.session.userID], (error, results, fields) => {
        if(error) throw error;
        if(results.length > 0){
            const userName = results[0].user_name;
            const userDeposit = results[0].user_deposit;
            res.render('userInfoPage', {userName : userName, userDeposit : userDeposit});
        }
        else{
            res.send('사용자를 찾을 수 없습니다.');
        }
    })
});

module.exports = router;