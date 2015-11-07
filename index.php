<!DOCTYPE html>
<html>
<head>	
	<title>Room Choice</title>
	<meta charset="UTF-8">
	<script type="text/javascript" src="http://code.jquery.com/jquery-latest.min.js"></script>
	<script type="text/JavaScript" src="js/select_room.js"></script>
	<link href='https://fonts.googleapis.com/css?family=Slabo+27px' rel='stylesheet' type='text/css'>
	<link rel='stylesheet' type='text/css' href='css/bootstrap.min.css'>
	<link rel='stylesheet' type='text/css' href='css/bootstrap-theme.min.css'>
	<link rel='stylesheet' type='text/css' href='css/main.css'>
</head>
<body>
<div class="container">
	<div class="row title">
		<div class="col-md-2">
			<img src="images/aechlogo.png" class="img-responsive" alt="AEC hackathon logo">
  		</div>
		<div class="col-md-9 text-right">
			<h1>Design Your Room.</h1>
		</div>
	</div>
	<div class="row">
		<div class="col-md-12">
			<div class="form-group">
  			<label for="sel1">Select room design:</label>
  			<select class="form-control" onchange="getval(this);">>
    		<option value="living_room">Classic Living Room</option>
    		<option value="bedroom">Stylish Bedroom</option>
  			</select>
  			</div>
  		</div>
  	</div>
  	<div class="row images">
  		<div class="col-md-5">
  			<img id="img_room" src="images/living_room.jpg" class="img-responsive" alt="Modern dining room">
  		</div>
  		<div class="col-md-5">
  			<img id="map_room" src="images/living_room_map.jpg" class="img-responsive" alt="Modern dining room">
  		</div>
  	</div>
</div>
</body>
</html>