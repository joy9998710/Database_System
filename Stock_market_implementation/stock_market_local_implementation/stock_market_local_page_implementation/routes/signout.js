const express = require('express');
const router = express.Router();
const db = require('../db');

router.get('/', (req, res) => {
    res.render('signoutPage')
});

router.post('/', (req, res) => {
    const {userID, userPassword} = req.body;
    if(userID !== req.session.userID){
        return res.render('signoutPage', {WrongUserInfo : 'wrong'});
    }
    if(userPassword !== req.session.userPassword){
        return res.render('signoutPage', {WrongUserInfo : 'wrong'});
    }
    db.query("DELETE FROM user WHERE user_id = ?", [userID], (error, results, fields) => {
        if(error) throw error;
        return res.render('home', {signoutValue : 'success'});
    });
})

module.exports = router;