$(document).ready(
    function () {
	$("nav#TOC a").click( function () {
	    $($(this).attr("href")).animate({"background-color": "rgba(255,255,0,1)"}, function() {$(this).animate({"background-color": "rgba(255,255,255,0)"});});
	});
    }
);
