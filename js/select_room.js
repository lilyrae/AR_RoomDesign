function getval(sel) {
    var room = sel.value;

	var request = $.ajax({
			        type:  'get',
			        cache:  false ,
			        url:  'selectRoom.php',
			        data:  {roomName: room}
			      });

	var image = document.getElementById("img_room");
    image.src = 'images/' + room + '.jpg';

    var map = document.getElementById("map_room");
    map.src = 'images/' + room + '_map.jpg';

}