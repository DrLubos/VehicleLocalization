import React from "react";
import { Box, Button, useDisclosure, useToast } from "@chakra-ui/react";
import VehicleBoxList from "./VehicleBoxList";
import VehicleFormModal from "./VehicleFormModal";

const Sidepanel = ({ vehicles, refreshVehicles, onDelete, onUpdate, onShowOnMap }) => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  const toast = useToast();

  const handleAddVehicle = async (data) => {
    try {
      const response = await fetch("https://webapi.vehiclemap.xyz/vehicles", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Authorization: `Bearer ${localStorage.getItem("authToken")}`,
        },
        body: JSON.stringify(data),
      });
      if (!response.ok) {
        throw new Error(`Failed to add vehicle with status ${response.status}`);
      }
    } catch (error) {
      console.error("Failed to add vehicle:", error);
      toast({
        title: "Failed to add vehicle",
        description: error.message + ". Make sure you entered valid data.",
        status: "error",
        duration: 5000,
        isClosable: true,
      });
      return;
    }
    toast({
      title: "Vehicle added",
      description: "Vehicle has been successfully added.",
      status: "success",
      duration: 3000,
      isClosable: true,
    });
    onClose();
    refreshVehicles();
  };

  return (
    <Box
      w={{ base: "100%", md: "400px" }}
      p={4}
      maxH="calc(100vh - 74px)"
      minH="calc(100vh - 74px)"
      overflowY="auto"
      sx={{
        borderRight: "1px solid",
        borderRightColor: "gray.700",
        borderImage: "linear-gradient(to bottom, gray.700, transparent) 1",
      }}
    >
      <Button colorScheme="blue" mb={4} w="full" onClick={onOpen}>
        + Add Vehicle
      </Button>
      <VehicleBoxList
        vehicles={vehicles}
        onDelete={onDelete}
        onUpdate={onUpdate}
        onShowOnMap={onShowOnMap}
      />
      <VehicleFormModal
        isOpen={isOpen}
        onClose={onClose}
        onSubmit={handleAddVehicle}
        isEdit={false}
      />
    </Box>
  );
};

export default Sidepanel;
