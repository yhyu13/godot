@tool
extends Node

static var _p = null

func _ready():
	_p = Player.new()
	_p.hp = 7
	print("before reload hp=", _p.hp)
	var gm = Engine.get_singleton("GDExtensionManager")
	var status = gm.reload_extension("res://self_rule.gdextension")
	print("reload status=", status)
	print("after reload hp=", _p.hp)
	get_tree().quit()
