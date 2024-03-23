function init() {
    var source = new EventSource("/get_updates");

    source.onmessage = function(event) {
        document.getElementById("val").innerHTML = event.data;
    };
}
