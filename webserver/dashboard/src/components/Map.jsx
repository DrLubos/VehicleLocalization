import React, { useEffect } from "react";
import { Box } from "@chakra-ui/react";
import { MapContainer, TileLayer, Marker, Popup, useMap } from "react-leaflet";
import "leaflet/dist/leaflet.css";
import L from "leaflet";

const createSVGIcon = (color) => {
  const rawSVG = `
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 500 820">
  <path
    d="M250,0 C163,0 90,73 90,160 C90,221 135,290 160,330 L250,500 L340,330 C365,290 410,221 410,160 C410,73 337,0 250,0 Z"
    fill="CURRENT_COLOR"
  />
</svg>
`;
  const svgWithColor = rawSVG.replace("CURRENT_COLOR", color);
  const encoded = encodeURIComponent(svgWithColor)
    .replace(/%20/g, " ")
    .replace(/%3D/g, "=")
    .replace(/%3A/g, ":")
    .replace(/%2F/g, "/");
  const dataUrl = `data:image/svg+xml;charset=UTF-8,${encoded}`;
  return L.icon({
    iconUrl: dataUrl,
    iconSize: [40, 65],
    iconAnchor: [20, 40],
  });
};

const formatTimestamp = (timestamp) => {
  const dateObj = new Date(timestamp);
  const day = String(dateObj.getUTCDate()).padStart(2, "0");
  const month = String(dateObj.getUTCMonth() + 1).padStart(2, "0");
  const year = dateObj.getUTCFullYear();
  const hours = String(dateObj.getUTCHours()).padStart(2, "0");
  const minutes = String(dateObj.getUTCMinutes()).padStart(2, "0");
  return `${day}.${month} ${hours}:${minutes} UTC`;
};

const MapViewUpdater = ({ selectedVehicle }) => {
  const map = useMap();
  useEffect(() => {
    if (selectedVehicle && selectedVehicle.last_position) {
      const lat = selectedVehicle.last_position.latitude;
      const lon = selectedVehicle.last_position.longitude;
      map.setView([lat, lon], 16);
    }
  }, [selectedVehicle, map]);
  return null;
};

const Map = ({ vehicles, selectedVehicle }) => {
  const defaultCenter = [48.1486, 17.1077];
  const center =
    vehicles.find((veh) => veh.last_position)
      ? [
          vehicles.find((veh) => veh.last_position).last_position.latitude,
          vehicles.find((veh) => veh.last_position).last_position.longitude,
        ]
      : defaultCenter;
  return (
    <Box w="100%" h="100%">
      <MapContainer center={center} zoom={13} style={{ height: "100%", width: "100%" }}>
        <TileLayer
          attribution='&copy; <a href="https://www.openstreetmap.org">OpenStreetMap</a> contributors'
          url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        />
        {vehicles.map(
          (veh) =>
            veh.last_position && (
              <Marker
                key={veh.id}
                position={[veh.last_position.latitude, veh.last_position.longitude]}
                icon={createSVGIcon(veh.color)}
              >
                <Popup>
                  <strong>{veh.name}</strong>
                  <br />
                  Last seen:{" "}
                  {formatTimestamp(veh.last_position.location_time)}
                  <br />
                  Speed: {veh.last_position.speed} km/h
                </Popup>
              </Marker>
            )
        )}
        <MapViewUpdater selectedVehicle={selectedVehicle} />
      </MapContainer>
    </Box>
  );
};

export default Map;
