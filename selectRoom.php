<?php
    if (($sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP)) === false) {
      echo "socket_create() failed: reason: " . socket_strerror(socket_last_error()) . "\n";
    }

    $msg =  $_GET['roomName'];
    $len = strlen($msg);

    $address = '192.168.168.35';
    $port = 9999;

    $result = socket_connect($sock, $address, $port);

    socket_write($sock, $msg, $len);

    socket_close($sock);

?>