#!/usr/bin/expect

spawn ./FlexRAN-FEC-SDK-19-04.sh
#expect "enter number:"
send "q"
expect "That you agree to the above software license"
send -- "Y\r"
expect off
