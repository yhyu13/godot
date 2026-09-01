extends Node

func _ready():
	var p = Player.new()
	print("CREATED player")
	var gm = Engine.get_singleton("GDExtensionManager")
	var status = gm.unload_extension("res://self_rule.gdextension")
	print("unload status=", status)
	p.free()
	print("FREED player (unreachable if crash)")
	get_tree().quit()
