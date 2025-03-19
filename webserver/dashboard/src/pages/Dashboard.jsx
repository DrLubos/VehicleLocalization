import React, { useEffect, useState } from "react";
import { Box, Flex, useColorModeValue } from "@chakra-ui/react";
import Navbar from "../components/Navbar";
import Sidepanel from "../components/Sidepanel";
import Map from "../components/Map";

const Dashboard = () => {
  const [vehicles, setVehicles] = useState([]);
  const [selectedVehicle, setSelectedVehicle] = useState(null);
  const bg = useColorModeValue("gray.50", "gray.900");

  const refreshVehicles = () => {
    const token = localStorage.getItem("authToken");
    if (!token) return;
    fetch("https://webapi.vehiclemap.xyz/vehicles/last-position", {
      headers: { Authorization: `Bearer ${token}` },
    })
      .then((response) => {
        if (!response.ok) throw new Error(`Error fetching vehicles: ${response.status}`);
        return response.json();
      })
      .then((data) => {
        const transformed = data.map((item) => ({
          ...item.vehicle,
          last_position: item.last_position,
        }));
        setVehicles(transformed);
      })
      .catch((err) => console.error(err));
  };

  useEffect(() => {
    refreshVehicles();
  }, []);

  const handleDelete = (id) => {
    setVehicles((prev) => prev.filter((v) => v.id !== id));
  };

  const handleUpdate = (updatedVehicle) => {
    setVehicles((prev) =>
      prev.map((v) => (v.id === updatedVehicle.id ? updatedVehicle : v))
    );
  };

  const handleShowOnMap = (vehicle) => {
    setSelectedVehicle(vehicle);
  };

  return (
    <Box minH="100vh" bg={bg}>
      <Navbar username={localStorage.getItem("username") || "User"} />
      <Flex>
        <Sidepanel
          vehicles={vehicles}
          refreshVehicles={refreshVehicles}
          onDelete={handleDelete}
          onUpdate={handleUpdate}
          onShowOnMap={handleShowOnMap}
        />
        <Box flex="1" h="calc(100vh - 74px)">
          <Map vehicles={vehicles} selectedVehicle={selectedVehicle} />
        </Box>
      </Flex>
    </Box>
  );
};

export default Dashboard;
