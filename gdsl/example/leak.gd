extends Node

static var _leaked_player = null

func _ready():
	_leaked_player = Player.new()
	print("CREATED player hp=", _leaked_player.hp)
	get_tree().quit()
