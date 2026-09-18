map="$1"
game_dir="$REDALERT_DIR"

#
echo "CNCMaps.Renderer uses near identical codepaths to the game to render it wil be exporting a full size pixel perfect map"
mono "$game_dir/CNCMaps/CNCMaps.Renderer.exe" -i "$map" --force-yr -p -m "$game_dir" -d screens/ -o "capture_$(date +%Y-%m-%d_%H-%M-%S)"
