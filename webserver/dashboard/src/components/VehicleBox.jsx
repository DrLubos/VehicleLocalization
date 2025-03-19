import React, { useState, useEffect } from "react";
import {
  Box,
  Flex,
  Text,
  IconButton,
  Collapse,
  Button,
  useToast,
  Modal,
  ModalOverlay,
  ModalContent,
  ModalHeader,
  ModalCloseButton,
  ModalBody,
  ModalFooter,
  useDisclosure,
} from "@chakra-ui/react";
import {
  ChevronDownIcon,
  DeleteIcon,
  EditIcon,
  CheckIcon,
  CloseIcon,
  InfoIcon,
  RepeatIcon,
} from "@chakra-ui/icons";
import VehicleFormModal from "./VehicleFormModal";
import RouteBoxList from "./RouteBoxList";

function darkenHex(hex, percent = 20) {
  let num = parseInt(hex.replace("#", ""), 16);
  let r = (num >> 16) - Math.round((num >> 16) * (percent / 100));
  let g = ((num >> 8) & 0x00ff) - Math.round(((num >> 8) & 0x00ff) * (percent / 100));
  let b = (num & 0x0000ff) - Math.round((num & 0x0000ff) * (percent / 100));
  r = r < 0 ? 0 : r;
  g = g < 0 ? 0 : g;
  b = b < 0 ? 0 : b;
  return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1).toUpperCase();
}

const formatTimestamp = (timestamp) => {
  const dateObj = new Date(timestamp);
  const day = String(dateObj.getUTCDate()).padStart(2, "0");
  const month = String(dateObj.getUTCMonth() + 1).padStart(2, "0");
  const year = dateObj.getUTCFullYear();
  const hours = String(dateObj.getUTCHours()).padStart(2, "0");
  const minutes = String(dateObj.getUTCMinutes()).padStart(2, "0");
  return `${day}.${month}.${year} ${hours}:${minutes} UTC`;
};

const VehicleBox = ({ vehicle, onDelete, onUpdate, onShowOnMap }) => {
  const [localStatus, setLocalStatus] = useState(vehicle.status);
  const [isOpen, setIsOpen] = useState(false);
  const [routes, setRoutes] = useState(null);
  const [loadingRoutes, setLoadingRoutes] = useState(false);
  const toast = useToast();

  const {
    isOpen: isEditOpen,
    onOpen: onEditOpen,
    onClose: onEditClose,
  } = useDisclosure();

  const {
    isOpen: isDeleteOpen,
    onOpen: onDeleteOpen,
    onClose: onDeleteClose,
  } = useDisclosure();

  useEffect(() => {
    if (isOpen && routes === null) {
      fetchRoutes();
    }
  }, [isOpen]);

  const fetchRoutes = async () => {
    setLoadingRoutes(true);
    try {
      const token = localStorage.getItem("authToken");
      const response = await fetch(
        `https://webapi.vehiclemap.xyz/vehicles/${vehicle.id}/routes`,
        { headers: { Authorization: `Bearer ${token}` } }
      );
      if (!response.ok) {
        throw new Error(`Error fetching routes: ${response.status}`);
      }
      const data = await response.json();
      setRoutes(data);
    } catch (error) {
      console.error(error);
      toast({
        title: "Error",
        description: "Could not load routes.",
        status: "error",
        duration: 5000,
        isClosable: true,
      });
    }
    setLoadingRoutes(false);
  };

  const toggleDropdown = () => {
    setIsOpen(!isOpen);
  };

  const handleDeleteConfirm = async () => {
    try {
      const response = await fetch(
        `https://webapi.vehiclemap.xyz/vehicles/${vehicle.id}`,
        {
          method: "DELETE",
          headers: {
            Authorization: `Bearer ${localStorage.getItem("authToken")}`,
          },
        }
      );
      if (!response.ok) {
        throw new Error(`Error deleting vehicle: ${response.status}`);
      }
      toast({
        title: "Vehicle deleted",
        description: "Vehicle has been removed.",
        status: "success",
        duration: 3000,
        isClosable: true,
      });
      if (onDelete) onDelete(vehicle.id);
    } catch (error) {
      console.error(error);
      toast({
        title: "Error",
        description: "Could not delete vehicle.",
        status: "error",
        duration: 5000,
        isClosable: true,
      });
    }
  };

  const handleDelete = (e) => {
    e.stopPropagation();
    onDeleteOpen();
  };

  const handleEditSubmit = async (data) => {
    try {
      const response = await fetch(
        `https://webapi.vehiclemap.xyz/vehicles/${vehicle.id}`,
        {
          method: "PUT",
          headers: {
            "Content-Type": "application/json",
            Authorization: `Bearer ${localStorage.getItem("authToken")}`,
          },
          body: JSON.stringify(data),
        }
      );
      if (!response.ok) {
        throw new Error(`Error updating vehicle: ${response.status}`);
      }
      const updatedVehicle = await response.json();
      setLocalStatus(updatedVehicle.status);
      toast({
        title: "Vehicle updated",
        description: "Vehicle has been updated successfully.",
        status: "success",
        duration: 3000,
        isClosable: true,
      });
      onEditClose();
      if (onUpdate) onUpdate({ ...vehicle, ...updatedVehicle });
    } catch (error) {
      console.error(error);
      toast({
        title: "Error",
        description: "Could not update vehicle.",
        status: "error",
        duration: 5000,
        isClosable: true,
      });
    }
  };

  const handleToggleStatus = async (e) => {
    e.stopPropagation();
    try {
      const response = await fetch(
        `https://webapi.vehiclemap.xyz/vehicles/${vehicle.id}/togle-status`,
        {
          method: "PUT",
          headers: {
            "Content-Type": "application/json",
            Authorization: `Bearer ${localStorage.getItem("authToken")}`,
          },
        }
      );
      if (!response.ok) {
        throw new Error(`Error toggling status: ${response.status}`);
      }
      const data = await response.json();
      const newStatus =
        data.new_status ||
        (data.message.toLowerCase().includes("inactive") ? "inactive" : "active");
      setLocalStatus(newStatus);
      toast({
        title: "Status updated",
        description: data.message,
        status: "success",
        duration: 3000,
        isClosable: true,
      });
      if (onUpdate) onUpdate({ ...vehicle, status: newStatus });
    } catch (error) {
      console.error(error);
      toast({
        title: "Error",
        description: "Could not update status.",
        status: "error",
        duration: 5000,
        isClosable: true,
      });
    }
  };

  const handleShowOnMapClick = (e) => {
    e.stopPropagation();
    if (onShowOnMap) {
      onShowOnMap({ ...vehicle });
    }
  };

  return (
    <Box borderWidth="1px" borderRadius="md" p={2} cursor="pointer" mb={0} onClick={toggleDropdown}>
      <Flex align="center">
        <Box
          w="40px"
          h="40px"
          bg={
            localStatus === "inactive"
              ? darkenHex(vehicle.color, 40)
              : vehicle.color
          }
          borderRadius="md"
          mr={3}
          display="flex"
          alignItems="center"
          justifyContent="center"
          position="relative"
          cursor="pointer"
          onClick={(e) => {
            e.stopPropagation();
            fetchRoutes();
          }}
          title="Click to refresh routes"
        >
          <RepeatIcon boxSize="5" color="white" />
        </Box>
        <Box flex="1">
          <Text fontWeight="bold">{vehicle.name}</Text>
          {vehicle.last_position && (
            <Text fontSize="xs" color="gray.500">
              ({vehicle.last_position.latitude.toFixed(4)},{" "}
              {vehicle.last_position.longitude.toFixed(4)})<br />
              {formatTimestamp(vehicle.last_position.location_time)}
            </Text>
          )}
        </Box>
        <IconButton
          icon={
            <ChevronDownIcon
              transform={isOpen ? "rotate(180deg)" : "rotate(0deg)"}
              transition="transform 0.3s ease"
            />
          }
          variant="ghost"
          aria-label="Toggle Options"
          onClick={(e) => {
            e.stopPropagation();
            toggleDropdown();
          }}
        />
      </Flex>
      <Collapse in={isOpen} animateOpacity>
        <Flex mt={2} justify="space-between" gap={2}>
          <Button
            size="sm"
            variant="outline"
            flex={1}
            height="60px"
            p={0}
            display="flex"
            flexDirection="column"
            alignItems="center"
            justifyContent="center"
            onClick={(e) => {
              e.stopPropagation();
              handleDelete(e);
            }}
          >
            <DeleteIcon boxSize="5" mb="1" />
            <Text fontSize="xs">Delete</Text>
          </Button>
          <Button
            size="sm"
            variant="outline"
            flex={1}
            height="60px"
            p={0}
            display="flex"
            flexDirection="column"
            alignItems="center"
            justifyContent="center"
            onClick={(e) => {
              e.stopPropagation();
              onEditOpen();
            }}
          >
            <EditIcon boxSize="5" mb="1" />
            <Text fontSize="xs">Edit</Text>
          </Button>
          <Button
            size="sm"
            variant="outline"
            flex={1}
            height="60px"
            p={0}
            display="flex"
            flexDirection="column"
            alignItems="center"
            justifyContent="center"
            onClick={(e) => {
              e.stopPropagation();
              handleToggleStatus(e);
            }}
          >
            {localStatus === "active" || localStatus === "registered" ? (
              <>
                <CloseIcon boxSize="5" mb="1" />
                <Text fontSize="xs" textAlign="center">Deactivate</Text>
              </>
            ) : (
              <>
                <CheckIcon boxSize="5" mb="1" />
                <Text fontSize="xs" textAlign="center">Activate</Text>
              </>
            )}
          </Button>
          <Button
            size="sm"
            variant="outline"
            flex={1}
            height="60px"
            p={0}
            display="flex"
            flexDirection="column"
            alignItems="center"
            justifyContent="center"
            onClick={handleShowOnMapClick}
          >
            <InfoIcon boxSize="5" mb="1" />
            <Text fontSize="xs" textAlign="center">
              Show
              <br />
              on Map
            </Text>
          </Button>
        </Flex>
        {routes && (
          <Box mt={2}>
            <RouteBoxList routes={routes} onDeleteRoute={() => {}} onShowRoute={() => {}} />
          </Box>
        )}
      </Collapse>

      <VehicleFormModal
        isOpen={isEditOpen}
        onClose={onEditClose}
        onSubmit={handleEditSubmit}
        initialData={{
          name: vehicle.name,
          imei: vehicle.imei,
          color: vehicle.color,
          position_check_freq: vehicle.position_check_freq,
          min_distance_delta: vehicle.min_distance_delta,
          max_idle_minutes: vehicle.max_idle_minutes,
          manual_route_start_enabled: vehicle.manual_route_start_enabled,
        }}
        isEdit={true}
      />

      <Modal isOpen={isDeleteOpen} onClose={onDeleteClose} isCentered>
        <ModalOverlay />
        <ModalContent>
          <ModalHeader>Delete Vehicle</ModalHeader>
          <ModalCloseButton />
          <ModalBody>
            <Text>Are you sure you want to delete vehicle "{vehicle.name}"?</Text>
          </ModalBody>
          <ModalFooter>
            <Button colorScheme="red" mr={3} onClick={handleDeleteConfirm}>
              Delete
            </Button>
            <Button variant="ghost" onClick={onDeleteClose}>
              Cancel
            </Button>
          </ModalFooter>
        </ModalContent>
      </Modal>
    </Box>
  );
};

export default VehicleBox;
